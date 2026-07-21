#define NOMINMAX
#include <iostream>
#include "tts.h"


int main() {

	TTSConfig tts_config;
	// tts_config.speaker_id = 1;
	// tts_config.encoder_path = "/home/fiboai/MeloTTS/onnx_models/250613/zh_en/model_encoder.onnx";
	// tts_config.decoder_path = "/home/fiboai/MeloTTS/onnx_models/250613/zh_en/model_decoder_static.onnx";
	// tts_config.use_bert = false;

	TNConfig tn_config;
	TTS tts(tts_config, tn_config);
	std::string text = "语音识别是实现人机交互的一种重要途径，是自然语言处理的基础环节，随着人工智能技术的发展，人机交互等大量应用场景存在着流式语音识别的需求。";
	tts.inference(text);
	
	return 0;
}
