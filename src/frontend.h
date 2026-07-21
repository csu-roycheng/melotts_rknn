#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <assert.h>
#include "tone_sandhi.h"
#include "cppjieba/Jieba.hpp"
#include "cppinyin/cppinyin.h"


inline std::vector<std::string> split (const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;
    while (getline (ss, item, delim)) {
        result.push_back (item);
    }
    return result;
}


struct JiebaConfig {
    std::string DICT_PATH = "/root/TTS/cpp/src/cppjieba/dict/jieba.dict.utf8";
    std::string HMM_PATH = "/root/TTS/cpp/src/cppjieba/dict/hmm_model.utf8";
    std::string IDF_PATH = "/root/TTS/cpp/src/cppjieba/dict/idf.utf8";
    std::string USER_DICT_PATH = "/root/TTS/cpp/src/cppjieba/dict/user.dict.utf8";
    std::string STOP_WORD_PATH = "/root/TTS/cpp/src/cppjieba/dict/stop_words.utf8";
};


struct PinyinConfig {
    std::string PINYIN_DICT_PATH = "/root/TTS/cpp/src/cppinyin/pinyin.raw";
    std::string PINYIN_TO_SYMBOL_PATH = "/root/TTS/cpp/assets/opencpop-strict.txt";
};


struct FrontendConfig {
    std::string frontend_lang;
    // 音素相关
    std::string TOKENS_PATH = "/root/TTS/cpp/assets/tokens1.txt";
    JiebaConfig jieba_config;
    PinyinConfig pinyin_config;
};



class TextFrontend {
private:

    std::unique_ptr<cppjieba::Jieba> jieba;
    std::unique_ptr<cppinyin::PinyinEncoder> pinyin;

    std::unordered_map<std::string, std::pair<std::vector<int>, std::vector<int>>> lexicon;
    const std::unordered_map<std::string, int> language_map = {
        {"ZH", 0},
        {"JP", 1},
        {"EN", 2},
        {"ZH_MIX_EN", 3},
        {"KR", 4},
        {"ES", 5},
        {"SP", 5},
        {"FR", 6},
        {"YUE", 7}
    };
    const std::unordered_set<std::string> punctuation{ "!", "?", "…", ",", ".", "'", "-" };
    const std::unordered_set<char> simple_initials = { 'b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'g', 'k',
                                                      'h', 'j', 'q', 'x', 'r', 'z', 'c', 's', 'y', 'w' };
    const std::unordered_set<std::string> compound_initials = { "zh", "ch", "sh" };
    const std::unordered_map<std::string, std::string> v_rep_map = {
        {"uei", "ui"},
        {"iou", "iu"},
        {"uen", "un"},
    };
    std::unordered_map<std::string, int> tokens;
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> pinyin_to_symbol_map;

    std::unique_ptr<ToneSandhi> tone_sandhi;


public:

    TextFrontend(const FrontendConfig& frontend_config);

    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> readPinyinFile(const std::string& filepath);

    std::vector<std::string> splitEachChar(const std::string& text);

    bool is_english(std::string s);

    std::vector<std::string> merge_english(const std::vector<std::string>& splitted_text);

    void intersperse(std::vector<int>& phones, std::vector<int>& tones, std::vector<int>& language);

    std::pair<std::string, std::string> split_initials_finals(const std::string& raw_pinyin);

    std::pair<std::vector<std::string>, std::vector<std::string>> _get_initials_finals(const std::string& input);

    std::tuple<std::vector<std::string>, std::vector<int64_t>, std::vector<int>> _chinese_g2p(
        std::vector<std::pair<std::string, std::string>>& segments);


    void convert(const std::string& text, 
        std::vector<int>& phones, 
        std::vector<int>& tones, 
        std::vector<int>& word2ph, 
        std::vector<int>& language, 
        const std::string& language_str="ZH");
};