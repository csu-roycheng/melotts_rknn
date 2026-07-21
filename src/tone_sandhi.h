#pragma once
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include "cppjieba/Jieba.hpp"

inline auto _all_tone_three = [](const std::vector<std::string>& finals) {
    return std::all_of(finals.begin(), finals.end(), [](const std::string& f) {
        return f.back() == '3';
    });
};

class ToneSandhi {
private:
    const std::set<std::string> numeric =
    { "零", "一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "百", "千", "万", "亿", "兆" };
    const std::unordered_set<std::string> must_not_neural_tone_words =
    { "男子", "女子", "分子", "原子", "量子", "莲子", "石子", "瓜子", "电子", "人人", "虎虎" };
    const std::unordered_set<std::string> must_neural_tone_words = {
        "麻烦", "麻利", "鸳鸯", "高粱", "骨头", "骆驼", "马虎", "首饰", "馒头", "馄饨", "风筝", "难为", "队伍", "阔气",
        "闺女", "门道", "锄头", "铺盖", "铃铛", "铁匠", "钥匙", "里脊", "里头", "部分", "那么", "道士", "造化", "迷糊",
        "连累", "这么", "这个", "运气", "过去", "软和", "转悠", "踏实", "跳蚤", "跟头", "趔趄", "财主", "豆腐", "讲究",
        "记性", "记号", "认识", "规矩", "见识", "裁缝", "补丁", "衣裳", "衣服", "衙门", "街坊", "行李", "行当", "蛤蟆",
        "蘑菇", "薄荷", "葫芦", "葡萄", "萝卜", "荸荠", "苗条", "苗头", "苍蝇", "芝麻", "舒服", "舒坦", "舌头", "自在",
        "膏药", "脾气", "脑袋", "脊梁", "能耐", "胳膊", "胭脂", "胡萝", "胡琴", "胡同", "聪明", "耽误", "耽搁", "耷拉",
        "耳朵", "老爷", "老实", "老婆", "老头", "老太", "翻腾", "罗嗦", "罐头", "编辑", "结实", "红火", "累赘", "糨糊",
        "糊涂", "精神", "粮食", "簸箕", "篱笆", "算计", "算盘", "答应", "笤帚", "笑语", "笑话", "窟窿", "窝囊", "窗户",
        "稳当", "稀罕", "称呼", "秧歌", "秀气", "秀才", "福气", "祖宗", "砚台", "码头", "石榴", "石头", "石匠", "知识",
        "眼睛", "眯缝", "眨巴", "眉毛", "相声", "盘算", "白净", "痢疾", "痛快", "疟疾", "疙瘩", "疏忽", "畜生", "生意",
        "甘蔗", "琵琶", "琢磨", "琉璃", "玻璃", "玫瑰", "玄乎", "狐狸", "状元", "特务", "牲口", "牙碜", "牌楼", "爽快",
        "爱人", "热闹", "烧饼", "烟筒", "烂糊", "点心", "炊帚", "灯笼", "火候", "漂亮", "滑溜", "溜达", "温和", "清楚",
        "消息", "浪头", "活泼", "比方", "正经", "欺负", "模糊", "槟榔", "棺材", "棒槌", "棉花", "核桃", "栅栏", "柴火",
        "架势", "枕头", "枇杷", "机灵", "本事", "木头", "木匠", "朋友", "月饼", "月亮", "暖和", "明白", "时候", "新鲜",
        "故事", "收拾", "收成", "提防", "挖苦", "挑剔", "指甲", "指头", "拾掇", "拳头", "拨弄", "招牌", "招呼", "抬举",
        "护士", "折腾", "扫帚", "打量", "打算", "打点", "打扮", "打听", "打发", "扎实", "扁担", "戒指", "懒得", "意识",
        "意思", "情形", "悟性", "怪物", "思量", "怎么", "念头", "念叨", "快活", "忙活", "志气", "心思", "得罪", "张罗",
        "弟兄", "开通", "应酬", "庄稼", "干事", "帮手", "帐篷", "希罕", "师父", "师傅", "巴结", "巴掌", "差事", "工夫",
        "岁数", "屁股", "尾巴", "少爷", "小气", "小伙", "将就", "对头", "对付", "寡妇", "家伙", "客气", "实在", "官司",
        "学问", "学生", "字号", "嫁妆", "媳妇", "媒人", "婆家", "娘家", "委屈", "姑娘", "姐夫", "妯娌", "妥当", "妖精",
        "奴才", "女婿", "头发", "太阳", "大爷", "大方", "大意", "大夫", "多少", "多么", "外甥", "壮实", "地道", "地方",
        "在乎", "困难", "嘴巴", "嘱咐", "嘟囔", "嘀咕", "喜欢", "喇嘛", "喇叭", "商量", "唾沫", "哑巴", "哈欠", "哆嗦",
        "咳嗽", "和尚", "告诉", "告示", "含糊", "吓唬", "后头", "名字", "名堂", "合同", "吆喝", "叫唤", "口袋", "厚道",
        "厉害", "千斤", "包袱", "包涵", "匀称", "勤快", "动静", "动弹", "功夫", "力气", "前头", "刺猬", "刺激", "别扭",
        "利落", "利索", "利害", "分析", "出息", "凑合", "凉快", "冷战", "冤枉", "冒失", "养活", "关系", "先生", "兄弟",
        "便宜", "使唤", "佩服", "作坊", "体面", "位置", "似的", "伙计", "休息", "什么", "人家", "亲戚", "亲家", "交情",
        "云彩", "事情", "买卖", "主意", "丫头", "丧气", "两口", "东西", "东家", "世故", "不由", "不在", "下水", "下巴",
        "上头", "上司", "丈夫", "丈人", "一辈", "那个", "菩萨", "父亲", "母亲", "咕噜", "邋遢", "费用", "冤家", "甜头",
        "介绍", "荒唐", "大人", "泥鳅", "幸福", "熟悉", "计划", "扑腾", "蜡烛", "姥爷", "照顾", "喉咙", "吉他", "弄堂",
        "蚂蚱", "凤凰", "拖沓", "寒碜", "糟蹋", "倒腾", "报复", "逻辑", "盘缠", "喽啰", "牢骚", "咖喱", "扫把", "惦记",
    };
    const std::unordered_set<char> punctuations = {
        ',',
        '.',
        '!',
        '?',
        ';',
        '-',
        '\'' 
    };


    std::vector<std::string> split_utf8_chinese(const std::string& str) {
        std::vector<std::string> res;  // chinese characters
        int strSize = str.size();
        int i = 0;

        while (i < strSize) {
            int len = 1;
            for (int j = 0; j < 6 && (str[i] & (0x80 >> j)); j++) {
                len = j + 1;
            }
            res.push_back(str.substr(i, len));
            i += len;
        }
        return res;
    }


    void _bu_sandhi(const std::vector<std::string>& chinese_characters, std::vector<std::string>& sub_finals) {
        if (chinese_characters.size() == 3 && chinese_characters[1] == "不") {
            sub_finals[1].back() = '5';
            return;
        }
        for (size_t i = 0; i < chinese_characters.size(); i++) {
            auto& c = chinese_characters[i];
            if (c == "不" && i != chinese_characters.size() - 1 && sub_finals[i + 1].back() == '4') {
                sub_finals[i].back() = '2';
            }
        }
    }


    void _yi_sandhi(const std::vector<std::string>& chinese_characters, std::vector<std::string>& sub_finals) {
        if (is_numeric(chinese_characters)) {
            return;
        }
        if (chinese_characters.size() == 3 && chinese_characters[1] == "一" &&
            chinese_characters[0] == chinese_characters[2]) {
            sub_finals[1].back() = '5';
            return;
        }
        if (chinese_characters[0] == "第" && chinese_characters[1] == "一")  // word.startswith("第一"):
            return;

        for (size_t i = 0; i < chinese_characters.size(); i++) {
            auto& c = chinese_characters[i];
            if (c == "一" && i != chinese_characters.size() - 1) {
                if ((sub_finals[i + 1].back()) == '4') {
                    sub_finals[i].back() = '2';
                }
                else {
                    sub_finals[i].back() = '1';
                }
            }
        }
    }


    void _neural_sandhi(const std::string& word,
        const std::vector<std::string>& chinese_characters,
        const std::string& pos,
        cppjieba::Jieba* jieba,
        std::vector<std::string>& sub_finals) {
        int n = chinese_characters.size();
        if (n < 2 || must_not_neural_tone_words.find(word) != must_not_neural_tone_words.end())
            return;

        std::string temp = chinese_characters[n - 2] + chinese_characters.back();

        if (must_neural_tone_words.find(temp) != must_neural_tone_words.end()) {
            sub_finals.back().back() = '5';
        }
        for (size_t i = 1; i < n; i++) {
            auto& c = chinese_characters[i];
            if (chinese_characters[i] == chinese_characters[i - 1] && (pos[0] == 'n' || pos[0] == 'v' || pos[0] == 'a')) {
                sub_finals[i].back() = '5';
            }
        }
        const static std::unordered_set<std::string> nenuralCharSet = { "吧", "呢", "啊", "呐", "噻", "嘛", "吖", "嗨",
                                                                       "呐", "哦", "哒", "额", "滴", "哩", "哟", "喽",
                                                                       "啰", "耶", "喔", "诶", "的", "地", "得" };
        static const std::unordered_set<std::string> st1 = { "上", "下", "进", "出", "回", "过", "起", "开" };
        static const std::unordered_set<std::string> st2 = { "几", "有", "两", "半", "多", "各", "整", "每", "做", "是" };
        if (nenuralCharSet.find(chinese_characters.back()) != nenuralCharSet.end()) {
            sub_finals.back().back() = '5';
        }
        else if ((chinese_characters.back() == "们" || chinese_characters.back() == "字") &&
            (pos[0] == 'n' || pos[0] == 'r')) {
            sub_finals.back().back() = '5';
        }
        else if ((chinese_characters.back() == "上" || chinese_characters.back() == "下" ||
            chinese_characters.back() == "里")  // e.g. 桌上, 地下, 家里
            && (pos[0] == 's' || pos[0] == 'l' || pos[0] == 'f')) {
            sub_finals.back().back() = '5';
        }
        else if ((chinese_characters.back() == "来" || chinese_characters.back() == "去") &&
            st1.find(chinese_characters[n - 2]) != st1.end()) {  // e.g. 上来, 下去
            sub_finals.back().back() = '5';
        }
        auto it = std::find(chinese_characters.begin(), chinese_characters.end(), "个");
        // "个"做量词
        if (it != chinese_characters.end() && it != chinese_characters.begin() &&
            (is_numeric(*(it - 1)) || st2.find(*(it - 1)) != st2.end())) {
            sub_finals[it - chinese_characters.begin()].back() = '5';
        }
        auto slices = _split_word(word, jieba);
        if (slices == 2) {
            auto temp = chinese_characters.front() + chinese_characters[1];
            if (must_neural_tone_words.find(temp) != must_neural_tone_words.end()) {
                sub_finals[slices - 1].back() = '5';
            }
        }
    }


    void _three_sandhi(const std::string& word,
        const std::vector<std::string>& chinese_characters,
        cppjieba::Jieba* jieba,
        std::vector<std::string>& sub_finals) {
        if (chinese_characters.size() == 2 && sub_finals[0].back() == '3' && sub_finals[1].back() == '3') {
            sub_finals[0].back() = '2';
            return;
        }
        if (chinese_characters.size() == 3) {
            size_t slices = _split_word(word, jieba);
            if (_all_tone_three(sub_finals)) {
                if (slices == 2) {
                    sub_finals[0].back() = sub_finals[1].back() = '2';
                }
                else if (slices == 1) {
                    sub_finals[1].back() = '2';
                }
            }
            else {
                if (sub_finals[1].back() != '3') {
                    return;
                }
                if (sub_finals[0].back() != '3' && sub_finals[2].back() != '3') {
                    return;
                }
                if (sub_finals[0].back() == '3') {
                    sub_finals[0].back() = '2';
                    return;
                }
                if (slices == 1) {
                    sub_finals[1].back() = '2';
                }
            }
        }

        if (chinese_characters.size() == 4) {
            if (sub_finals[0].back() == '3' && sub_finals[1].back() == '3') {
                sub_finals[0].back() = '2';
            }
            if (sub_finals[2].back() == '3' && sub_finals[3].back() == '3') {
                sub_finals[2].back() = '2';
            }
        }
    }


    std::vector<std::pair<std::string, std::string>> _merge_yi(std::vector<std::pair<std::string, std::string>>& seg) {
        std::vector<std::pair<std::string, std::string>> new_seg;
        int n = seg.size();
        // function 1 and function2
        bool if_continue = false;  // skip second "听", of "听一听"
        for (int i = 0; i < n; ++i) {
            if (if_continue) {
                if_continue = false;
                continue;
            }
            auto& word = seg[i].first;
            auto& pos = seg[i].second;
            if (i >= 1 && word == "一" && i + 1 < n && seg[i - 1].first == seg[i + 1].first && seg[i - 1].second == "v") {
                new_seg.back().first += "一" + seg[i + 1].first;
                if_continue = true;
            }
            else if (new_seg.size() && new_seg.back().first == "一") {
                new_seg.back().first += word;
            }
            else
                new_seg.emplace_back(word, pos);
        }
        return new_seg;
    }


    std::vector<std::pair<std::string, std::string>> _merge_chinese_patterns(
        std::vector<std::pair<std::string, std::string>>& seg) {

        std::vector<std::pair<std::string, std::string>> new_seg;

        for (auto& seg_pair : seg) {
            auto& word = seg_pair.first;
            auto& pos = seg_pair.second;
            if (new_seg.size() &&
                (word == new_seg.back().first && punctuations.find(word.front()) == punctuations.end() || new_seg.back().first == "不"))
                new_seg.back().first += word;
            else if (new_seg.size() && word == "儿")  //_merge_er
                new_seg.back().first += "儿";
            else
                new_seg.emplace_back(std::move(word), std::move(pos));
        }
        if (new_seg.size() && new_seg.back().first == "不")
            new_seg.back().second = "d";

        return new_seg;
    }


    inline bool is_numeric(const std::string& chinese_character) {
        if (numeric.find(chinese_character) == numeric.end())
            return false;
        return true;
    }

    inline bool is_numeric(const std::vector<std::string>& chinese_characters) {
        return std::all_of(chinese_characters.begin(), chinese_characters.end(), [this](const std::string& ch) {
            return is_numeric(ch);
            });
    }

    inline size_t _split_word(const std::string& word, cppjieba::Jieba* jieba) {
        std::vector<cppjieba::Word> words;
        jieba->CutForSearch(word, words);
        cppjieba::Word wordForSearch = words[0];
        if (wordForSearch.unicode_offset == 0) {
            if (wordForSearch.unicode_length != word.size()) {
                return wordForSearch.unicode_length;
            }
        }
        else {
            return wordForSearch.unicode_offset;
        }
        return 0;
    }


public:
    std::vector<std::pair<std::string, std::string>> pre_merge_for_modify(
        std::vector<std::pair<std::string, std::string>>& seg) {
        auto seg1 = _merge_yi(seg);
        return _merge_chinese_patterns(seg1);
    }

    void modified_tone(const std::string& word,
        const std::string& tag,
        cppjieba::Jieba* jieba,
        std::vector<std::string>& sub_finals) {
        // 此处需要 汉语分字 假设这里进入的是utf-8纯汉字无标点
        std::vector<std::string> chinese_characters = split_utf8_chinese(word);
        _bu_sandhi(chinese_characters, sub_finals);
        _yi_sandhi(chinese_characters, sub_finals);
        _neural_sandhi(word, chinese_characters, tag, jieba, sub_finals);
        _three_sandhi(word, chinese_characters, jieba, sub_finals);
    }
    
};
