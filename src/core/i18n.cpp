#include "i18n.h"
#include "core/str_utils.h"
#include <cstdlib>
#include <algorithm>

namespace tmoe {
    std::unordered_map<std::string, std::string> I18n::current_dict_;
    std::string I18n::current_lang_ = "en_US";
    std::vector<std::string> I18n::available_langs_;
    std::unordered_set<std::string> I18n::missing_keys_;
    std::unordered_set<std::string> I18n::used_keys_;
    bool I18n::collect_missing_ = false;

    void I18n::init(std::string_view lang) {
        current_dict_.clear();
        missing_keys_.clear();
        used_keys_.clear();

        // 环境变量 TMOE_I18N_CHECK 开启缺失 key 收集
        const char *check_env = std::getenv("TMOE_I18N_CHECK");
        collect_missing_ = (check_env && check_env[0] == '1');

        if (collect_missing_) {
            std::atexit([] {
                dump_missing_keys();
            });
        }

        std::string target(lang);
        size_t dot_pos = target.find('.');
        if (dot_pos != std::string::npos) {
            target = target.substr(0, dot_pos);
        }

        using namespace i18n::embedded;
        available_langs_.clear();
        for (auto const &[l, _]: LANGS) {
            available_langs_.push_back(std::string(l));
        }

        auto it = LANGS.find(target);
        if (it == LANGS.end()) {
            it = LANGS.find("en_US");
            if (it == LANGS.end() && !LANGS.empty()) it = LANGS.begin();
        }

        if (it != LANGS.end()) {
            current_lang_ = std::string(it->first);
            try {
                auto j = nlohmann::json::parse(it->second);
                for (auto &[k, v]: j.items()) {
                    if (v.is_string()) {
                        current_dict_[k] = v.get<std::string>();
                    }
                }
            } catch (const nlohmann::json::parse_error &e) {
                std::cerr << "[TMOE I18n Error] 嵌入式语言包解析失败: " << e.what() << "\n";
            }
        } else {
            current_lang_ = "en_US";
        }
    }

    std::string I18n::tr(std::string_view key) {
        std::string k(key);
        auto it = current_dict_.find(k);
        if (it != current_dict_.end()) {
            if (collect_missing_) used_keys_.insert(k);
            return it->second;
        }
        if (collect_missing_) {
            missing_keys_.insert(k);
            used_keys_.insert(k);
        }
        return k;
    }

    std::vector<std::string> I18n::missing_keys() {
        std::vector<std::string> result(missing_keys_.begin(), missing_keys_.end());
        std::sort(result.begin(), result.end());
        return result;
    }

    void I18n::dump_missing_keys() {
        // 1. 缺失的 key（代码用了但 JSON 没有）
        bool has_missing = !missing_keys_.empty();
        if (has_missing) {
            std::cerr << "\n══════════════════════════════════════════\n"
                      << "[TMOE I18n] 缺失翻译 (" << missing_keys_.size() << " 个)"
                      << " — 代码用了但 JSON 里没有:\n"
                      << "══════════════════════════════════════════\n";
            auto keys = missing_keys();
            for (const auto &k : keys) {
                std::cerr << "  \"" << k << "\": \"\",\n";
            }
        }

        // 2. 未使用的 key（JSON 有但代码没用过）
        std::vector<std::string> unused;
        for (const auto &[k, _] : current_dict_) {
            if (used_keys_.find(k) == used_keys_.end()) {
                unused.push_back(k);
            }
        }
        std::sort(unused.begin(), unused.end());

        if (!unused.empty()) {
            std::cerr << "\n══════════════════════════════════════════\n"
                      << "[TMOE I18n] 未使用翻译 (" << unused.size() << " 个)"
                      << " — JSON 有但代码没用过:\n"
                      << "══════════════════════════════════════════\n";
            for (const auto &k : unused) {
                std::cerr << "  \"" << k << "\",\n";
            }
            std::cerr << "══════════════════════════════════════════\n"
                      << "[TMOE I18n] ↑ 以上 key 可安全从 JSON 中删除\n";
        }

        if (!has_missing && unused.empty()) {
            std::cerr << "[TMOE I18n] ✓ 所有 key 均已翻译且无冗余。\n";
        }

        std::cerr << "\n";
    }

    std::string_view I18n::current_lang() {
        return current_lang_;
    }

    const std::vector<std::string> &I18n::available_langs() {
        return available_langs_;
    }

    void I18n::register_custom(
        std::string_view lang,
        const std::unordered_map<std::string, std::string> &dict) {
        if (current_lang_ == lang) {
            for (auto const &[k, v]: dict) {
                current_dict_[k] = v;
            }
        }
    }

    std::string I18n::format(std::string tmpl,
                             const std::vector<std::string> &args) {
        for (size_t i = 0; i < args.size(); ++i) {
            std::string placeholder = "{" + std::to_string(i) + "}";
            tmpl = replace_all(std::move(tmpl), placeholder, args[i]);
        }
        return tmpl;
    }
} // namespace tmoe
