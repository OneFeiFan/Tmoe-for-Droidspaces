#ifndef STR_UTILS_H
#define STR_UTILS_H
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <algorithm>

namespace tmoe {

/** 去除字符串末尾的换行符 (\n, \r)。
 *  项目中有 30+ 处通过 while 循环手动裁剪，统一收口到此函数。
 *  返回字符串自身引用以支持链式调用。 */
    inline std::string &trim_newline(std::string &s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        return s;
    }

/** trim_newline 的右值重载（返回新字符串，不修改原值）。 */
    inline std::string trim_newline(std::string &&s) {
        trim_newline(s);
        return s;
    }

/** 去除字符串首尾空白字符（空格、制表、换行、回车）。 */
    inline std::string &trim(std::string &s) {
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
        return s;
    }

    inline std::string trim(std::string &&s) {
        trim(s);
        return s;
    }

/** 按分隔符切分字符串。相邻分隔符产生空串，与 getline 行为一致。 */
    inline std::vector<std::string> split(std::string_view s, char delim) {
        std::vector<std::string> result;
        std::istringstream iss(std::string{s});
        std::string token;
        while (std::getline(iss, token, delim))
            result.push_back(std::move(token));
        return result;
    }

/** 按分隔符拼接字符串序列。 */
    inline std::string join(const std::vector<std::string> &parts, std::string_view delim) {
        std::string result;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += delim;
            result += parts[i];
        }
        return result;
    }

/** 按分隔符拼接 string_view 序列（重载，避免临时对象）。 */
    inline std::string join(const std::vector<std::string_view> &parts, std::string_view delim) {
        std::string result;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += delim;
            result += parts[i];
        }
        return result;
    }

/** 检查字符串是否以指定前缀开头。 */
    inline bool starts_with(std::string_view s, std::string_view prefix) {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    }

/** 检查字符串是否以指定后缀结尾。 */
    inline bool ends_with(std::string_view s, std::string_view suffix) {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

/** 检查字符串是否包含子串。 */
    inline bool contains(std::string_view s, std::string_view sub) {
        return s.find(sub) != std::string_view::npos;
    }

/** char 重载：检查字符串是否包含单个字符。 */
    inline bool contains(std::string_view s, char c) {
        return s.find(c) != std::string_view::npos;
    }

/** 搜索并替换所有出现的子串（不可变版本 — 返回新字符串）。 */
    inline std::string replace_all(std::string s, std::string_view from, std::string_view to) {
        if (from.empty()) return s;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
        return s;
    }

/** 不区分大小写的字符串相等比较。 */
    inline bool iequals(std::string_view a, std::string_view b) {
        return a.size() == b.size() &&
               std::equal(a.begin(), a.end(), b.begin(),
                          [](char ca, char cb) { return std::tolower(ca) == std::tolower(cb); });
    }

} // namespace tmoe
#endif //STR_UTILS_H
