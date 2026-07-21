#include "tokenizer.h"


BERTTokenizer::BERTTokenizer()
{
    // 默认构造函数不加载词表，成员变量初始化为安全值
    unk_id_ = cls_id_ = sep_id_ = pad_id_ = 0;
}

BERTTokenizer::BERTTokenizer(const std::string& vocab_file, bool do_lower_case)
{
    load_vocab(vocab_file);
    unk_id_ = get_id("[UNK]");
    cls_id_ = get_id("[CLS]");
    sep_id_ = get_id("[SEP]");
    pad_id_ = get_id("[PAD]");
    do_lower_case_ = do_lower_case;
}


Encoding BERTTokenizer::encode(const std::string& text, int max_length) const 
{
    auto chars = utf8_chars(text);
    std::vector<int64_t> ids;
    ids.push_back(cls_id_);

    for (const auto& ch : chars) {
        std::string token = ch;
        if (do_lower_case_ && is_ascii_alpha(token)) {
            std::transform(token.begin(), token.end(), token.begin(), ::tolower);
        }
        ids.push_back(get_id(token));
    }

    ids.push_back(sep_id_);

    if (static_cast<int>(ids.size()) > max_length) {
        ids.resize(max_length);
    }

    Encoding out;
    out.input_ids = ids;
    out.attention_mask = std::vector<int64_t>(ids.size(), 1);
    out.token_type_ids = std::vector<int64_t>(ids.size(), 0); // 单句

    return out;
}

void BERTTokenizer::load_vocab(const std::string& path) 
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open vocab file: " + path);
    }
    std::string line;
    int id = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vocab_[line] = id++;
    }
    file.close();
}


int BERTTokenizer::get_id(const std::string& token) const 
{
    auto it = vocab_.find(token);
    return (it != vocab_.end()) ? it->second : unk_id_;
}


bool BERTTokenizer::is_ascii_alpha(const std::string& s) const 
{
    if (s.size() != 1) return false;
    unsigned char c = s[0];
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}


std::vector<std::string> BERTTokenizer::utf8_chars(const std::string& str) const 
{
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t char_len = 1;
        if ((c & 0xF0) == 0xF0) char_len = 4;
        else if ((c & 0xE0) == 0xE0) char_len = 3;
        else if ((c & 0xC0) == 0xC0) char_len = 2;
        // else 1 byte (ASCII)

        if (i + char_len > str.length()) break; // 防止越界

        chars.push_back(str.substr(i, char_len));
        i += char_len;
    }
    return chars;
}