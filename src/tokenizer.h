#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <cctype>
#include <algorithm>


struct Encoding {
    std::vector<int64_t> input_ids;
    std::vector<int64_t> attention_mask;
    std::vector<int64_t> token_type_ids;
};

class BERTTokenizer {
public:
    BERTTokenizer();

    BERTTokenizer(const std::string& vocab_file, bool do_lower_case = true);

    Encoding encode(const std::string& text, int max_length = 512) const;

private:
    std::unordered_map<std::string, int> vocab_;
    bool do_lower_case_;
    int unk_id_, cls_id_, sep_id_, pad_id_;

    void load_vocab(const std::string& path);

    int get_id(const std::string& token) const;

    // 判断是否为 ASCII 字母（A-Z a-z）
    bool is_ascii_alpha(const std::string& s) const;

    // 将 UTF-8 字符串拆分为单个字符（支持中文）
    std::vector<std::string> utf8_chars(const std::string& str) const;

};