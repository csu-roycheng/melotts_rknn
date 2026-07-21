#include "frontend.h"


TextFrontend::TextFrontend(const FrontendConfig& frontend_config)
{

    // if (frontend_lang == "ZH") {

    // }
    tone_sandhi = std::make_unique<ToneSandhi>();

    jieba = std::make_unique<cppjieba::Jieba>(
        frontend_config.jieba_config.DICT_PATH, 
        frontend_config.jieba_config.HMM_PATH, 
        frontend_config.jieba_config.USER_DICT_PATH, 
        frontend_config.jieba_config.IDF_PATH, 
        frontend_config.jieba_config.STOP_WORD_PATH
    );

    std::cout << "cppjieba Module Init Done!!!" << std::endl;

    pinyin = std::make_unique<cppinyin::PinyinEncoder>(frontend_config.pinyin_config.PINYIN_DICT_PATH);
    std::cout << "cppinyin Module Init Done!!!" << std::endl;

    pinyin_to_symbol_map = readPinyinFile(frontend_config.pinyin_config.PINYIN_TO_SYMBOL_PATH); // 完整拼音到声母韵母的映射
    
    // load pinyin to ids
    std::ifstream ifs(frontend_config.TOKENS_PATH);
    assert(ifs.is_open());

    std::string line;
    while ( std::getline(ifs, line) ) {
        auto splitted_line = split(line, ' ');
        tokens.insert({splitted_line[0], std::stoi(splitted_line[1])});
    }
    ifs.close();
}



std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> TextFrontend::readPinyinFile(const std::string& filepath) 
{
    auto pinyin_to_symbol_map = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filepath << std::endl;
        return pinyin_to_symbol_map;
    }
    // format is  key tab v1 space v2
    std::string line;
    while (std::getline(file, line)) {
        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            std::string pinyin = line.substr(0, tabPos);
            std::string symbols = line.substr(tabPos + 1);
            std::istringstream iss(symbols);
            std::vector<std::string> symbolsVec;
            std::string symbol;
            while (iss >> symbol) {
                symbolsVec.push_back(symbol);
            }
            (*pinyin_to_symbol_map)[pinyin] = symbolsVec;
        }
    }

    file.close();
    std::cout << "load opencpop-strict.txt to pinyin_to_symbol_map, containing " << pinyin_to_symbol_map->size() << " keys." << std::endl;
    return pinyin_to_symbol_map;
}


std::vector<std::string> TextFrontend::splitEachChar(const std::string& text)
{
    std::vector<std::string> words;
    std::string input(text);
    int len = input.length();
    int i = 0;
    
    while (i < len) {
        int next = 1;
        if ((input[i] & 0x80) == 0x00) {
        } else if ((input[i] & 0xE0) == 0xC0) {
            next = 2;
        } else if ((input[i] & 0xF0) == 0xE0) {
            next = 3;
        } else if ((input[i] & 0xF8) == 0xF0) {
            next = 4;
        }
        words.push_back(input.substr(i, next));
        i += next;
    }
    return words;
}


bool TextFrontend::is_english(std::string s) 
{
    if (s.size() == 1)
        return (s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z');
    else
        return false;
}



std::vector<std::string> TextFrontend::merge_english(const std::vector<std::string>& splitted_text) 
{
    std::vector<std::string> words;
    int i = 0;
    while (i < splitted_text.size()) {
        std::string s;
        if (is_english(splitted_text[i])) {
            while (i < splitted_text.size()) {
                if (!is_english(splitted_text[i])) {
                    break;
                }
                s += splitted_text[i];
                i++;
            }
            // to lowercase
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c){ return std::tolower(c); });
            words.push_back(s);
            if (i >= splitted_text.size())
                break;
        }
        else {
            words.push_back(splitted_text[i]);
            i++;
        }
    }
    return words;
}


void TextFrontend::intersperse(std::vector<int>& phones, std::vector<int>& tones, std::vector<int>& language) 
{
    const size_t old_sz = phones.size();
    phones.resize(old_sz * 2 + 1, 0);
    tones.resize(old_sz * 2 + 1, 0);
    language.resize(old_sz * 2 + 1, 0);
    for (size_t i = old_sz; i-- > 0; ) {
        phones[i * 2 + 1] = std::move(phones[i]); phones[i] = 0;
        tones[i * 2 + 1] = std::move(tones[i]); tones[i] = 0;
        language[i * 2 + 1] = std::move(language[i]); language[i] = 0;
    }
}


std::pair<std::string, std::string> TextFrontend::split_initials_finals(const std::string& raw_pinyin) 
{
    int n = raw_pinyin.length();
    if (n == 0)
        return {};
    // check compound_initials
    if (n > 2 && compound_initials.find(raw_pinyin.substr(0, 2)) != compound_initials.end()) {
        return { raw_pinyin.substr(0, 2), raw_pinyin.substr(2) };
    }
    else if (simple_initials.find(raw_pinyin.front()) != simple_initials.end()) {
        return { raw_pinyin.substr(0, 1), raw_pinyin.substr(1) };
    }
    else {
        // 有些字没有声母 比如 玉 鹅
        return { "", raw_pinyin };
    }
    return {};
}


std::pair<std::vector<std::string>, std::vector<std::string>> TextFrontend::_get_initials_finals(
        const std::string& input) 
{

    std::vector<std::string> initials, finals;
    std::vector<std::string> pieces;

    pinyin->Encode(input, &pieces);

    for (const auto& piece : pieces) {
        if (punctuation.find(piece) != punctuation.end()) {  // if punctuation
            initials.emplace_back(piece);
            finals.emplace_back(piece);
        }
        else {
            //const auto& [orig_initial, orig_final] = split_initials_finals(piece);
            auto initial_final_pair = split_initials_finals(piece);
            std::string& orig_initial = initial_final_pair.first;
            std::string& orig_final = initial_final_pair.second;
            initials.emplace_back(orig_initial);

            // 的地得等轻声韵母没有声调数字，默认加上5表示轻声
            if (!orig_final.empty() && !std::isdigit(static_cast<unsigned char>(orig_final.back()))) {
                orig_final += '5'; 
            }
            finals.emplace_back(orig_final);
        }
    }
    return { initials, finals };
}


std::tuple<std::vector<std::string>, std::vector<int64_t>, std::vector<int>> TextFrontend::_chinese_g2p(
        std::vector<std::pair<std::string, std::string>>& segments) 
{

    auto new_segments = tone_sandhi->pre_merge_for_modify(segments);  // adjust word segmentation
    std::vector<std::string> phones_list;
    std::vector<int64_t> tones_list;
    std::vector<int> word2ph;

    for (const auto& seg_pair : new_segments) {

        auto word = seg_pair.first;
        auto tag = seg_pair.second;

        auto initials_finals_pair = _get_initials_finals(word);
        auto sub_initials = initials_finals_pair.first;
        auto sub_finals = initials_finals_pair.second;

        tone_sandhi->modified_tone(word, tag, jieba.get(), sub_finals);
        int n = sub_initials.size();
        assert(n == sub_finals.size());
        std::string pinyin;
        int tone = 0;
        
        for (int i = 0; i < n; ++i) {
            pinyin.clear();
            tone = 0;
            auto c = sub_initials[i];  // 声母 e.g. "w"
            auto v = sub_finals[i];    // 韵母+声调 "eng2"
            //std::cout << "raw pinyin: " << c << " " << v << std::endl;
            if (c == v) {              // punctuation
                word2ph.emplace_back(1);
                phones_list.emplace_back(c);
                tones_list.emplace_back(0);
            }
            else {
                tone = v.back() - '0';  // number for 声调
                v.pop_back();           // 韵母 without tone(声调)
                pinyin = c + v;
                
                assert(tone > 0 && tone <= 5);
                // 多音节
                if (v_rep_map.find(v) != v_rep_map.end()) {
                    pinyin = c + v_rep_map.at(v);
                }
                if (pinyin_to_symbol_map->find(pinyin) == pinyin_to_symbol_map->end())
                    std::cerr << "_chinese_g2p: " << pinyin << " not in map, " << word << "\n" << std::endl;
                const auto& phone = pinyin_to_symbol_map->at(pinyin);
                word2ph.emplace_back(phone.size());
                phones_list.insert(phones_list.end(), phone.begin(), phone.end());
                tones_list.insert(tones_list.end(), phone.size(), tone);
            }
        }
    }
    return { phones_list, tones_list, word2ph };
}


void TextFrontend::convert(const std::string& text, 
        std::vector<int>& phones, 
        std::vector<int>& tones, 
        std::vector<int>& word2ph, 
        std::vector<int>& language, 
        const std::string& language_str) 
{
    
    // 填充
    phones.push_back(0);
    tones.push_back(0);
    word2ph.push_back(3);
    int lang_id = language_map.at(language_str);
    language.push_back(lang_id);

    // 中英混合用
    //auto splitted_text = splitEachChar(text);
    //auto zh_mix_en = merge_english(splitted_text);


    // g2p
    std::vector<std::string> words;
    std::vector<std::pair<std::string, std::string>> tags;
    jieba->Tag(text, tags);

    const auto& g2p_result = _chinese_g2p(tags);

    for (const auto& ph : std::get<0>(g2p_result)) {
        phones.emplace_back(tokens[ph]);
    }
    for (const auto& tone : std::get<1>(g2p_result)) {
        tones.emplace_back(tone);
        language.emplace_back(lang_id);
    }
    for (const auto& w2p : std::get<2>(g2p_result)) {
        word2ph.emplace_back(w2p * 2);
    }

    phones.push_back(0);
    tones.push_back(0);
    word2ph.push_back(2);
    language.push_back(lang_id);
}