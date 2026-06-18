#include "terminal_app.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {

TerminalAppManager::TerminalAppManager(const TmoeConfig& cfg)
    : cfg_(cfg), family_(DistroFamily::Unknown) {
    family_ = infer_family_from_config(cfg_.linux_distro);
    if (family_ == DistroFamily::Unknown)
        family_ = PackageManager::detect_distro_family();
}

void TerminalAppManager::run_terminal_menu() {
    struct Entry { std::string key; std::string pkg; };
    const Entry list[] = {
        {"term.cool_retro_term", "cool-retro-term"},
        {"term.terminology",     "terminology"},
        {"term.terminator",      "terminator"},
        {"term.yakuake",         "yakuake"},
        {"term.guake",           "guake"},
        {"term.tilda",           "tilda"},
        {"term.konsole",         "konsole"},
        {"term.xfce4_terminal",  "xfce4-terminal"},
        {"term.gnome_terminal",  "gnome-terminal"},
        {"term.lxterminal",      "lxterminal"},
        {"term.mate_terminal",   "mate-terminal"},
        {"term.lilyterm",        "lilyterm"},
        {"term.sakura",          "sakura"},
        {"term.rxvt",            "rxvt"},
        {"term.stterm",          "stterm"},
        {"term.aterm",           "aterm"},
        {"term.xterm",           "xterm"},
    };

    std::string menu = cfg_.tui_bin + " --title \"" + _("term.menu_title")
        + "\" --menu \"" + _("term.menu_prompt") + "\" 0 0 0 ";
    for (size_t i = 0; i < sizeof(list)/sizeof(list[0]); ++i)
        menu += "\"" + std::to_string(i + 1) + "\" \"" + _(list[i].key) + "\" ";
    menu += "\"0\" \"" + _("menu.tui.back") + "\"";

    auto choice = Executor::tui_select(menu);
    if (choice.empty() || choice == "0") return;

    int idx = std::stoi(choice) - 1;
    if (idx >= 0 && idx < static_cast<int>(sizeof(list)/sizeof(list[0]))) {
        Logger::step(_(list[idx].key));
        PackageManager::install(list[idx].pkg, family_);
    }
}

} // namespace tmoe::domain
