#include "tts.h"


TTS::TTS(const TTSConfig& tts_config, const TNConfig& tn_config)
{

    model_language = tts_config.lang;
    sid[0] = tts_config.speaker_id;
    use_bert = tts_config.use_bert;
    ja_bert_dim = tts_config.bert_dim;

    frontend = std::make_unique<TextFrontend>(tts_config.frontend_config);
    std::cout << "===> TextFrontend Init Done!!!" << std::endl;
    
    text_normalizer = std::make_unique<wetext::Processor>(tn_config.tagger_path, tn_config.verbalizer_path);
    std::cout << "===> WeTextProcessing Init Done!!!" << std::endl;

    session_options.SetIntraOpNumThreads(4);
    session_options.SetInterOpNumThreads(4);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    encoder_session = std::make_shared<Ort::Session>(env, tts_config.encoder_path.c_str(), session_options);
    // decoder_session = std::make_shared<Ort::Session>(env, tts_config.decoder_path.c_str(), session_options);

    init_rknn_decoder(tts_config.decoder_path.c_str());

    if (use_bert) {
        tokenizer = std::make_unique<BERTTokenizer>(tts_config.vocab_path, true);
        bert_session = std::make_shared<Ort::Session>(env, tts_config.bert_path.c_str(), session_options);
        std::cout << "===> BERTTokenizer Init Done!!!" << std::endl;
    } else {
        std::cout << "===> No BERT Init!!!" << std::endl;
    }

    
    zp_slice.resize(192 * dec_len, 0.0f);
    bert_feat.resize(100 * 1024, 0.0f);
    final_audio.reserve(sample_rate * 10);
    std::cout << "Model Init Done" << std::endl;
}


std::vector<Ort::Value> TTS::bert_emb_extract(const std::string& text)
{
    Encoding encode = tokenizer->encode(text);
    size_t token_len = encode.input_ids.size();

    const int64_t bert_input_shape[2] = { 1, static_cast<int64_t>(token_len) };
    Ort::Value input_ids_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info, encode.input_ids.data(), token_len, bert_input_shape, 2);
    Ort::Value atten_mask_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info, encode.attention_mask.data(), token_len, bert_input_shape, 2);
    Ort::Value token_type_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info, encode.token_type_ids.data(), token_len, bert_input_shape, 2);

    std::vector<Ort::Value> bert_input_tensors;
    bert_input_tensors.emplace_back(std::move(input_ids_tensor));
    bert_input_tensors.emplace_back(std::move(token_type_tensor));
    bert_input_tensors.emplace_back(std::move(atten_mask_tensor));
    
    std::vector<Ort::Value> bert_output_tensors;
    try {
        auto start = std::chrono::high_resolution_clock::now();
        bert_output_tensors = bert_session->Run(Ort::RunOptions{ nullptr },
            bert_input_node_names.data(), bert_input_tensors.data(), bert_input_tensors.size(),
            bert_output_node_names.data(), bert_output_node_names.size());
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "BERT inference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "Error during albert inference: " << e.what() << std::endl;
    }

    return bert_output_tensors;
}


std::pair<std::vector<float>, int> TTS::repeat_emb(std::vector<Ort::Value>& outputs, const std::vector<int>& word2ph)
{
    int token_len = outputs[0].GetTensorTypeAndShapeInfo().GetShape()[1];
    float* hidden = outputs[0].GetTensorMutableData<float>(); // [token_len, 312]
    // repeat and transpose
    int repeat = 0;
    for (auto w : word2ph) {
        repeat += w;
    }

    std::vector<float> repeat_hidden;
    repeat_hidden.resize(100 * ja_bert_dim, 0.0f);
    if (repeat_hidden.size() < repeat * ja_bert_dim)
        repeat_hidden.resize(repeat * ja_bert_dim * 1.5, 0.0f);
    std::fill(repeat_hidden.begin(), repeat_hidden.begin() + repeat * ja_bert_dim, 0.0f);

    int col_idx = 0;
    for (size_t i = 0; i < token_len; ++i) {
        for (int rep = 0; rep < word2ph[i]; ++rep) {
            for (int c = 0; c < ja_bert_dim; ++c) {
                repeat_hidden[c * repeat + col_idx] = hidden[i * ja_bert_dim + c];
            }
            ++col_idx;
        }
    }
    return { repeat_hidden, repeat };
}



void TTS::inference(std::string& text) 
{

    text = text_normalizer->Normalize(text);

    size_t estimated_samples = text.size() * 1200;
    if (final_audio.size() < estimated_samples) {
        final_audio.reserve(estimated_samples * 1.2);
    }
    final_audio.clear();

    std::vector<std::string> sentences = split_sentence(text, 10, model_language);
    for (auto s : sentences) {
        std::cout << s << std::endl;
        phones.clear(); tones.clear(); word2ph.clear(); language.clear();

        frontend->convert(s, phones, tones, word2ph, language, "ZH_MIX_EN");
        frontend->intersperse(phones, tones, language);

        int repeat = 0;
        std::vector<float> repeat_hidden;
        if (use_bert) {
            auto bert_outputs = bert_emb_extract(s);
            auto repeat_pair = repeat_emb(bert_outputs, word2ph);
            repeat_hidden = repeat_pair.first;
            repeat = repeat_pair.second;
        } else {
            repeat = phones.size();
            repeat_hidden = std::vector<float>(repeat * ja_bert_dim, 0.0);
        }


        // encoder
        const int64_t x_input_shape[2] = { 1, repeat };
        const int64_t bert_feat_shape[3] = { 1, bert_dim, repeat };
        const int64_t ja_bert_feat_shape[3] = { 1, ja_bert_dim, repeat };
        std::vector<int> x_length = { repeat };

        Ort::Value x_tensor = Ort::Value::CreateTensor<int>(
            memory_info, phones.data(), repeat, x_input_shape, 2);
        Ort::Value x_length_tensor = Ort::Value::CreateTensor<int>(
            memory_info, x_length.data(), 1, x_len_input_shape, 1);
        Ort::Value tones_tensor = Ort::Value::CreateTensor<int>(
            memory_info, tones.data(), tones.size(), x_input_shape, 2);
        Ort::Value sid_tensor = Ort::Value::CreateTensor<int>(
            memory_info, sid.data(), 1, x_len_input_shape, 1);
        Ort::Value noise_scale_tensor = Ort::Value::CreateTensor<float>(
            memory_info, noise_scale.data(), 1, x_len_input_shape, 1);
        Ort::Value noise_scale_w_tensor = Ort::Value::CreateTensor<float>(
            memory_info, noise_scale_w.data(), 1, x_len_input_shape, 1);
        Ort::Value length_scale_tensor = Ort::Value::CreateTensor<float>(
            memory_info, length_scale.data(), 1, x_len_input_shape, 1);

        std::vector<Ort::Value> encoder_input_tensors;
        encoder_input_tensors.emplace_back(std::move(x_tensor));
        encoder_input_tensors.emplace_back(std::move(x_length_tensor));
        encoder_input_tensors.emplace_back(std::move(tones_tensor));
        encoder_input_tensors.emplace_back(std::move(sid_tensor));
        
        if (use_bert) {
            if (bert_feat.size() < repeat * bert_dim)
                bert_feat.resize(repeat * bert_dim * 1.5, 0.0f);
                
            Ort::Value bert_feat_tensor = Ort::Value::CreateTensor<float>(
                memory_info, bert_feat.data(), repeat * bert_dim, bert_feat_shape, 3);
            Ort::Value ja_bert_feat_tensor = Ort::Value::CreateTensor<float>(
                memory_info, repeat_hidden.data(), repeat * ja_bert_dim, ja_bert_feat_shape, 3);

            encoder_input_tensors.emplace_back(std::move(bert_feat_tensor));
            encoder_input_tensors.emplace_back(std::move(ja_bert_feat_tensor));
        }
        
        encoder_input_tensors.emplace_back(std::move(noise_scale_tensor));
        encoder_input_tensors.emplace_back(std::move(length_scale_tensor));
        encoder_input_tensors.emplace_back(std::move(noise_scale_w_tensor));
        
        std::vector<Ort::Value> encoder_output_tensors;
        try {
            auto start = std::chrono::high_resolution_clock::now();
            if (use_bert) {
                encoder_output_tensors = encoder_session->Run(Ort::RunOptions{ nullptr },
                    encoder_input_node_names.data(), encoder_input_tensors.data(), encoder_input_tensors.size(),
                    encoder_output_node_names.data(), encoder_output_node_names.size());
            } else {
                encoder_output_tensors = encoder_session->Run(Ort::RunOptions{ nullptr },
                    encoder_input_node_names_nobert.data(), encoder_input_tensors.data(), encoder_input_tensors.size(),
                    encoder_output_node_names.data(), encoder_output_node_names.size());
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << "Encoder inference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
        }
        catch (const Ort::Exception& e) {
            std::cerr << "Error during encoder inference: " << e.what() << std::endl;
            return;
        }

        std::vector<int64_t> zp_shape = encoder_output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        float* z_p = encoder_output_tensors[0].GetTensorMutableData<float>();
        float* g = encoder_output_tensors[1].GetTensorMutableData<float>();
        int* pronoun_len = encoder_output_tensors[2].GetTensorMutableData<int>();
        int* audio_len = encoder_output_tensors[3].GetTensorMutableData<int>();

        std::vector<int64_t> pronoun_len_shape = encoder_output_tensors[2].GetTensorTypeAndShapeInfo().GetShape();
        std::vector<int> word2pronoun = calc_word2pronoun(word2ph, pronoun_len);
        std::vector<std::vector<std::pair<int, int>> > slices = generate_slices(word2pronoun, dec_len);

        decoder_inputs[1].buf = g;
        std::vector<float> sub_audio_list;
        for (int i = 0; i < slices[0].size(); ++i) {
            std::fill(zp_slice.begin(), zp_slice.end(), 0.0f);
            int start = slices[1][i].first;
            int end = slices[1][i].second;

            for (int k = 0; k < 192; ++k) {
                int src_offset = k * zp_shape[2];
                int dst_offset = k * dec_len;
                std::memcpy(&zp_slice[dst_offset], &z_p[src_offset + start], (end - start) * sizeof(float));
            }

            decoder_inputs[0].buf = zp_slice.data();
            int ret = rknn_inputs_set(decoder_ctx, 2, decoder_inputs);
            if (ret < 0)
            {
                std::cerr << "rknn_inputs_set fail! ret=" << ret << std::endl;
                return;
            }
            
            auto decoder_start = std::chrono::high_resolution_clock::now();
            ret = rknn_run(decoder_ctx, NULL);
            if (ret < 0)
            {
                std::cerr << "rknn_run fail! ret=" << ret << std::endl;
                return;
            }
            auto decoder_end = std::chrono::high_resolution_clock::now();
            std::cout << "Decoder inference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(decoder_end - decoder_start).count() << " ms" << std::endl;

            ret = rknn_outputs_get(decoder_ctx, 1, decoder_outputs, NULL);
            if (ret < 0)
            {
                std::cerr << "rknn_outputs_get fail! ret=" << ret << std::endl;
                return;
            }

            float *decode_audio = reinterpret_cast<float *>(decoder_outputs[0].buf);
            post_process(sub_audio_list, decode_audio, slices, word2pronoun, i);
            rknn_outputs_release(decoder_ctx, 1, decoder_outputs);
        }
        final_audio.insert(final_audio.end(), sub_audio_list.begin(), sub_audio_list.end());
    }
    export_wav("output.wav", final_audio, sample_rate);
}



void TTS::post_process(
    std::vector<float>& sub_audio_list, 
    float* decode_audio,
    const std::vector<vector<pair<int, int>> >& slices,
    const std::vector<int>& word2pronoun,
    const int index)
{
    // float* audio = decoder_output_tensors[0].GetTensorMutableData<float>();

    // overlap
    int audio_start = 0;
    if (sub_audio_list.size() > 0 && slices[0][index - 1].second > slices[0][index].first) {
        audio_start = 512 * word2pronoun[slices[0][index].first];
    }
    int audio_end = 512 * (slices[1][index].second - slices[1][index].first);
    if (index < slices[0].size() - 1 && slices[0][index].second > slices[0][index + 1].first) {
        audio_end -= 512 * word2pronoun[slices[0][index].second - 1];
    }

    // log fade
    for (int k = 0; k < fade_samples; ++k) {
        decode_audio[audio_start + k] *= fade_in_envelope[k];
        decode_audio[audio_end - k] *= fade_out_envelope[fade_samples - k - 1];
    }

    sub_audio_list.insert(sub_audio_list.end(), decode_audio + audio_start, decode_audio + audio_end);
}



void TTS::export_wav(const std::string& Filename, const std::vector<float>& Data, unsigned SampleRate) 
{
    AudioFile<float>::AudioBuffer Buffer;
    Buffer.resize(1);

    Buffer[0] = Data;
    size_t BufSz = Data.size();

    AudioFile<float> File;
    File.setAudioBuffer(Buffer);
    File.setAudioBufferSize(1, (int)BufSz);
    File.setNumSamplesPerChannel((int)BufSz);
    File.setNumChannels(1);
    File.setBitDepth(16);
    File.setSampleRate(SampleRate);
    File.save(Filename, AudioFileFormat::Wave);
}


void TTS::init_rknn_decoder(const char* filename)
{
    decoder_model_data = load_model(filename, &decoder_data_size);

    int ret = rknn_init(&decoder_ctx, decoder_model_data, decoder_data_size, 0, NULL);
    if (ret < 0) {
        std::cerr << "rknn_init fail! ret=" << ret << std::endl;
        delete[] decoder_model_data;
        return;
    }

    // Get weight and internal mem size
    rknn_mem_size mem_size;
    ret = rknn_query(decoder_ctx, RKNN_QUERY_MEM_SIZE, &mem_size, sizeof(mem_size));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        return;
    }
    printf("total weight size: %d, total internal size: %d\n", mem_size.total_weight_size, mem_size.total_internal_size);
    
    // 设置输入数据
    std::memset(decoder_inputs, 0, sizeof(decoder_inputs));
    decoder_inputs[0].index = 0;
    decoder_inputs[0].type = RKNN_TENSOR_FLOAT32;
    decoder_inputs[0].size = 1 * 192 * 128 * sizeof(float);
    decoder_inputs[0].fmt = RKNN_TENSOR_NCHW;
    decoder_inputs[0].pass_through = 0;

    decoder_inputs[1].index = 1;
    decoder_inputs[1].type = RKNN_TENSOR_FLOAT32;
    decoder_inputs[1].size = 1 * 256 * 1 * sizeof(float);
    decoder_inputs[1].fmt = RKNN_TENSOR_NCHW;
    decoder_inputs[1].pass_through = 0;

    std::memset(decoder_outputs, 0, sizeof(decoder_outputs));
    decoder_outputs[0].want_float = 1;
}

unsigned char* TTS::load_model(const char* filename, int* model_size)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open model file: " << filename << std::endl;
        return nullptr;
    }

    file.seekg(0, std::ios::end);
    *model_size = file.tellg();
    file.seekg(0, std::ios::beg);

    unsigned char* model_data = new unsigned char[*model_size];
    file.read(reinterpret_cast<char*>(model_data), *model_size);
    file.close();

    return model_data;
}