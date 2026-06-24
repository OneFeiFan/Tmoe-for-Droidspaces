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
            std::string label;      // whiptail 显示标签 (与原版一致)
            std::string pkg;        // 包名
            std::string post_hint;  // 安装后提示 (空=无)
        };
        const Entry list[] = {
            {"cool-retro-term (穿梭时光,感受复古风,领略CRT的魅力)",         "cool-retro-term", "PPA: ppa:noobslab/apps"},
            {"Terminology (酷炫终端,支持预览图片和视频)",                    "terminology",     "tycat预览图片视频, tysend复制文件, tyls显示文件列表"},
            {"Terminator (支持分屏的终端)",                                 "terminator",      "Ctrl+Shift+O/E 切割"},
            {"Yakuake (基于KDE Konsole技术的下拉式终端)",                   "yakuake",         ""},
            {"Guake (GNOME下拉式终端)",                                    "guake",           ""},
            {"Tilda (雷神之锤风格的下拉式终端)",                             "tilda",           ""},
            {"Konsole (KDE默认终端)",                                      "konsole",         ""},
            {"Xfce4-Terminal (xfce默认终端)",                              "xfce4-terminal",  ""},
            {"Gnome-Terminal (gnome默认终端)",                             "gnome-terminal",  ""},
            {"LXTerminal (lxde默认终端)",                                  "lxterminal",      ""},
            {"Mate-Terminal (mate默认终端)",                               "mate-terminal",   ""},
            {"LilyTerm (基于libvte,追求轻量和快速)",                         "lilyterm",        ""},
            {"Sakura (如櫻花般優雅)",                                       "sakura",          ""},
            {"Rxvt (资源占用低,启动速度快)",                                 "rxvt",            ""},
            {"st (简单,清晰,且节约资源) → 启动命令: st 或 stterm",           "stterm",          "输 st 或 stterm 启动"},
            {"Aterm (基于rxvt,高度可定制) → 启动命令: aterm",               "aterm",            "输 aterm 启动"},
            {"xterm/Uxterm (X11标准指定的虚拟终端)",                         "xterm",           ""},
        };
        const int n = sizeof(list) / sizeof(list[0]);

        std::string menu = cfg_.tui_bin +
            " --title \"" + _("term.menu_title") + "\""
            " --menu \"" + _("term.menu_prompt") + "\" 0 50 0 ";
        for (int i = 0; i < n; ++i)
            menu += "\"" + std::to_string(i + 1) + "\" \"" + list[i].label + "\" ";
        menu += "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto choice = Executor::tui_select(menu);
        if (choice.empty() || choice == "0") break;

        int idx = std::stoi(choice) - 1;
        if (idx < 0 || idx >= n) continue;

        Logger::step(list[idx].label);
        PackageManager::install(list[idx].pkg, family_);

        // 安装后提示 (对应 Bash 的 case ${DEPENDENCY_02} in 分支)
        if (!list[idx].post_hint.empty()) {
            Logger::info(list[idx].post_hint);
        }
        if (list[idx].pkg == "cool-retro-term" && cfg_.linux_distro == "debian") {
            Logger::info("If install failed: sudo add-apt-repository ppa:noobslab/apps");
        }

        Logger::press_enter();
    }
}

} // namespace tmoe::domain
