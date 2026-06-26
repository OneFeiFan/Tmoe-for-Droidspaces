#include "domain/apps/terminal_app.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {

TerminalAppManager::TerminalAppManager(const TmoeConfig &cfg)
    : cfg_(cfg), family_(DistroFamily::Unknown) {
    family_ = infer_family_from_config(cfg_.linux_distro);
    if (family_ == DistroFamily::Unknown)
        family_ = PackageManager::detect_distro_family();
}

void TerminalAppManager::run_terminal_menu() {
    // 对应 Bash terminal (87行): tmoe_terminal_main_menu — 17项, 循环
    while (true) {
        struct Entry {
            std::string label_key;  // i18n key（whiptail 显示标签）
            std::string pkg;        // 包名
            std::string hint_key;   // i18n key（安装后提示，空=无）
        };
        const Entry list[] = {
            {"term.cool_retro_term",  "cool-retro-term", "term.hint_cool_retro_term"},
            {"term.terminology",      "terminology",     "term.hint_terminology"},
            {"term.terminator",       "terminator",      "term.hint_terminator"},
            {"term.yakuake",          "yakuake",         ""},
            {"term.guake",            "guake",           ""},
            {"term.tilda",            "tilda",           ""},
            {"term.konsole",          "konsole",         ""},
            {"term.xfce4_terminal",   "xfce4-terminal",  ""},
            {"term.gnome_terminal",   "gnome-terminal",  ""},
            {"term.lxterminal",       "lxterminal",      ""},
            {"term.mate_terminal",    "mate-terminal",   ""},
            {"term.lilyterm",         "lilyterm",        ""},
            {"term.sakura",           "sakura",          ""},
            {"term.rxvt",             "rxvt",            ""},
            {"term.st",               "stterm",          "term.hint_st"},
            {"term.aterm",            "aterm",           "term.hint_aterm"},
            {"term.xterm",            "xterm",           ""},
        };
        const int n = sizeof(list) / sizeof(list[0]);

        std::string menu = cfg_.tui_bin +
            " --title \"" + _("term.menu_title") + "\""
            " --menu \"" + _("term.menu_prompt") + "\" 0 50 0 ";
        for (int i = 0; i < n; ++i)
            menu += "\"" + std::to_string(i + 1) + "\" \"" + _(list[i].label_key) + "\" ";
        menu += "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto choice = Executor::tui_select(menu);
        if (choice.empty() || choice == "0") break;

        int idx = std::stoi(choice) - 1;
        if (idx < 0 || idx >= n) continue;

        Logger::step(_(list[idx].label_key));
        PackageManager::install(list[idx].pkg, family_);

        // 安装后提示 (对应 Bash 的 case ${DEPENDENCY_02} in 分支)
        if (!list[idx].hint_key.empty()) {
            Logger::info(_(list[idx].hint_key));
        }
        if (list[idx].pkg == "cool-retro-term" && cfg_.linux_distro == "debian") {
            Logger::info(_("term.hint_cool_retro_term_failed"));
        }

        Logger::press_enter();
    }
}

} // namespace tmoe::domain
