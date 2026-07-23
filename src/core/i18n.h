#pragma once

#include "i18n_data.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tmoe {
    /** 国际化翻译引擎。
     *
     *  用法:
     *    I18n::init("zh_CN");           // 加载翻译
     *    I18n::tr("container.install"); // → "正在安装容器..."
     *
     *  i18n 缺失检测:
     *    环境变量 TMOE_I18N_CHECK=1 → 收集所有缺失 key，进程退出时输出报告。
     *    也支持手动调用 I18n::dump_missing_keys() 在任何时机输出。
     */
    class I18n {
    public:
        static void init(std::string_view lang);

        /** 翻译一个 key。找不到时返回 key 本身，并记录到缺失集合。 */
        static std::string tr(std::string_view key);

        /** 带占位符 {0}, {1}, ... 的格式化翻译。 */
        template<typename... Args>
        static std::string trf(std::string_view key, Args &&... args);

        static std::string_view current_lang();

        static const std::vector<std::string> &available_langs();

        static void register_custom(std::string_view lang,
                                    const std::unordered_map<std::string, std::string> &dict);

        // ── 缺失 key 检测 ──
        /** 返回自 init 以来所有缺失的 key。 */
        static std::vector<std::string> missing_keys();

        /** 输出缺失 key 报告到 stderr。 */
        static void dump_missing_keys();

        /** 是否正在收集缺失 key。 */
        static bool is_collecting_missing() { return collect_missing_; }

    private:
        static std::unordered_map<std::string, std::string> current_dict_;
        static std::string current_lang_;
        static std::vector<std::string> available_langs_;
        static std::unordered_set<std::string> missing_keys_;
        static std::unordered_set<std::string> used_keys_;
        static bool collect_missing_;

        static std::string format(std::string tmpl,
                                  const std::vector<std::string> &args);
    };

    // ── 便捷宏 ──
#ifndef TMOE_NO_I18N_MACRO
#define _(key)       tmoe::I18n::tr(key)
#define _f(key, ...) tmoe::I18n::trf(key, __VA_ARGS__)
#endif
} // namespace tmoe

// ── 模板实现 ──
#include <sstream>
#include <regex>

namespace tmoe {
    template<typename... Args>
    std::string I18n::trf(std::string_view key, Args &&... args) {
        std::string tmpl = tr(key);
        if constexpr (sizeof...(args) == 0) return tmpl;
        std::vector<std::string> vec = {std::forward<Args>(args)...};
        return format(std::move(tmpl), vec);
    }
} // namespace tmoe
