#include "i18n.h"

namespace tmoe {
    std::unordered_map<std::string, std::string> I18n::current_dict_;
    std::string I18n::current_lang_ = "en_US";
    std::vector<std::string> I18n::available_langs_;

    void I18n::init(std::string_view lang) {
        current_dict_.clear();
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
            // 回退: en_US → 第一个可用语言
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
        auto it = current_dict_.find(std::string(key));
        if (it != current_dict_.end()) {
            return it->second;
        }
        return std::string(key);
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
            size_t pos = 0;
            while ((pos = tmpl.find(placeholder, pos)) != std::string::npos) {
                tmpl.replace(pos, placeholder.size(), args[i]);
                pos += args[i].size();
            }
        }
        return tmpl;
    }
} // namespace tmoe
