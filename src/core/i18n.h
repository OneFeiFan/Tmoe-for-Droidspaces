#pragma once
#include "i18n_data.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tmoe {

/** 国际化翻译引擎。
 *
 *  用法:
 *    I18n::init("zh_CN");           // 加载翻译
 *    I18n::tr("container.install"); // → "正在安装容器..."
 *
 *  扩展新语言: 在 locales/ 下放入 {lang}.json 即可，无需修改代码。
 */
class I18n {
public:
    /** 使用语言代码初始化（如 "zh_CN", "en_US"）。
     *  回退顺序: en_US → 第一个可用语言 → 空 (返回 key 本身)。
     */
    static void init(std::string_view lang);

    /** 翻译一个 key。找不到时返回 key 本身。 */
    static std::string tr(std::string_view key);

    /** 带占位符 {0}, {1}, ... 的格式化翻译。 */
    template<typename... Args>
    static std::string trf(std::string_view key, Args &&... args);

    /** 获取当前语言代码。 */
    static std::string_view current_lang();

    /** 列出所有可用语言代码。 */
    static const std::vector<std::string> &available_langs();

    /** 运行时注册自定义翻译（用于插件/动态扩展）。 */
    static void register_custom(std::string_view lang,
                                const std::unordered_map<std::string, std::string> &dict);

private:
    static std::unordered_map<std::string, std::string> current_dict_;
    static std::string current_lang_;
    static std::vector<std::string> available_langs_;

    /** 将 {n} 占位符替换为实际值。 */
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
