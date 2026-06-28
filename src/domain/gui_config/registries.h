#pragma once
#include <string>
#include <map>
#include <vector>

namespace tmoe::domain {
    struct DesktopInfo {
        std::string id;
        std::string name;
        std::string emoji;
        std::string session_cmd1;
        std::string session_cmd2;
        std::string display_manager;
        bool requires_root = false;
        bool is_window_manager = false;
        bool recommends_tiger_vnc = false;
        std::string pkg_group;
        std::string compat_notes;
        std::map<std::string, std::string> distro_pkgs;
    };

    namespace gui_config {
        // ═══════════════════════════════════════════════════════
        // 窗口管理器注册表 — 70+ WM名 → 包名映射
        // ═══════════════════════════════════════════════════════

        inline const std::map<std::string, std::string> &window_manager_packages() {
            static const std::map<std::string, std::string> wm = {
                {"openbox", "openbox obconf obmenu"},
                {"icewm", "icewm icewm-common"},
                {"fluxbox", "fluxbox"},
                {"jwm", "jwm"},
                {"fvwm", "fvwm fvwm-icons"},
                {"i3", "i3 i3status i3lock dmenu"},
                {"i3-gaps", "i3-gaps i3status i3lock dmenu"},
                {"awesome", "awesome"},
                {"xmonad", "xmonad xmobar"},
                {"bspwm", "bspwm sxhkd"},
                {"dwm", "dwm st"},
                {"herbstluftwm", "herbstluftwm"},
                {"qtile", "qtile"},
                {"spectrwm", "spectrwm"},
                {"sway", "sway swaylock swayidle"},
                {"hyprland", "hyprland"},
                {"enlightenment", "enlightenment"},
                {"cwm", "cwm"},
                {"wmii", "wmii"},
                {"ratpoison", "ratpoison"},
                {"stumpwm", "stumpwm"},
                {"blackbox", "blackbox"},
                {"pekwm", "pekwm"},
                {"windowmaker", "wmaker"},
                {"afterstep", "afterstep"},
                {"twm", "twm"},
                {"matchbox", "matchbox-window-manager"},
                {"compiz", "compiz compiz-core"},
                {"mutter", "mutter"},
                {"kwin", "kwin-x11"},
                {"9wm", "9wm"}, {"aewm", "aewm"}, {"aewm++", "aewm++"},
                {"clfswm", "clfswm"}, {"ctwm", "ctwm"}, {"evilwm", "evilwm"},
                {"flwm", "flwm"}, {"lwm", "lwm"}, {"marco", "marco"},
                {"metacity", "metacity"}, {"miwm", "miwm"}, {"muffin", "muffin"},
                {"mwm", "mwm"}, {"oroborus", "oroborus"}, {"sapphire", "sapphire"},
                {"sawfish", "sawfish"}, {"subtle", "subtle"}, {"sugar", "sucrose"},
                {"tinywm", "tinywm"}, {"ukwm", "ukwm"}, {"vdesk", "vdesk"},
                {"vtwm", "vtwm"}, {"w9wm", "w9wm"}, {"wm2", "wm2"},
                {"xfwm4", "xfwm4"}, {"exwm", "exwm"},
            };
            return wm;
        }

        // ═══════════════════════════════════════════════════════
        // GTK 主题名 → 包名映射
        // ═══════════════════════════════════════════════════════

        inline std::string theme_package_name(const std::string &name) {
            static const std::map<std::string, std::string> m = {
                {"arc", "arc-theme"},
                {"adapta", "adapta-gtk-theme"},
                {"numix", "numix-gtk-theme"},
                {"papirus", "papirus-icon-theme"},
                {"breeze", "breeze-gtk-theme"},
                {"materia", "materia-gtk-theme"},
            };
            auto it = m.find(name);
            return (it != m.end()) ? it->second : (name + "-theme");
        }

        // ═══════════════════════════════════════════════════════
        // 图标主题名 → 包名映射
        // ═══════════════════════════════════════════════════════

        inline std::string icon_theme_package_name(const std::string &name) {
            static const std::map<std::string, std::string> m = {
                {"papirus", "papirus-icon-theme"},
                {"numix", "numix-icon-theme"},
                {"breeze", "breeze-icon-theme"},
                {"tango", "tango-icon-theme"},
                {"adwaita", "adwaita-icon-theme"},
                {"elementary", "elementary-xfce-icon-theme"},
                {"moka", "moka-icon-theme"},
                {"faenza", "faenza-icon-theme"},
            };
            auto it = m.find(name);
            return (it != m.end()) ? it->second : (name + "-icon-theme");
        }

        // ═══════════════════════════════════════════════════════
        // 光标主题名 → 包名映射
        // ═══════════════════════════════════════════════════════

        inline std::string cursor_theme_package_name(const std::string &name) {
            static const std::map<std::string, std::string> m = {
                {"breeze", "breeze-cursor-theme"},
                {"chameleon", "chameleon-cursor-theme"},
            };
            auto it = m.find(name);
            return (it != m.end()) ? it->second : (name + "-cursor-theme");
        }

        // ═══════════════════════════════════════════════════════
        // 发行版 VNC 前置依赖表 (对应旧 Bash DEPENDENCY_02)
        // ═══════════════════════════════════════════════════════

        struct DistroGuiDeps {
            std::string vnc_pkg;
            std::string font_pkg;
            std::string extra_pkgs;
            bool skip_vnc = false; // Debian/Solus 不在此阶段装VNC
        };

        inline DistroGuiDeps distro_gui_deps(const std::string &family) {
            if (family == "debian") return {"", "fonts-noto-cjk", "dbus-x11", true};
            if (family == "redhat")
                return {
                    "tigervnc-server", "google-noto-sans-cjk-ttc-fonts google-noto-emoji-color-fonts", "", false
                };
            if (family == "arch") return {"tigervnc", "noto-fonts-cjk", "", false};
            if (family == "void") return {"tigervnc", "wqy-microhei", "xorg", false};
            if (family == "gentoo") return {"net-misc/tigervnc", "media-fonts/wqy-bitmapfont", "", false};
            if (family == "suse") return {"tigervnc-x11vnc", "noto-sans-sc-fonts", "perl-base", false};
            if (family == "solus") return {"", "font-noto-cjk", "dbus-launch", true};
            if (family == "alpine") return {"tigervnc", "font-noto-cjk", "dbus-x11 openrc", false};
            return {"tigervnc-standalone-server", "fonts-noto-cjk", "dbus-x11", false};
        }

        // ═══════════════════════════════════════════════════════
        // 完整桌面环境 + 窗口管理器注册表
        // ═══════════════════════════════════════════════════════

        inline const std::vector<DesktopInfo> &all_desktops() {
            static const std::vector<DesktopInfo> reg = {
                // ── 完整桌面环境 ──
                {
                    "xfce", "🐭 Xfce", "🐭", "xfce4-session", "startxfce4", "lightdm", false, false, true,
                    "xfce4 xfce4-goodies", "兼容性高,简单优雅",
                    {{"redhat","@xfce xfce*-plugin xfce4-panel-profiles"},{"gentoo","xfce4-meta x11-terms/xfce4-terminal"},{"suse","patterns-xfce-xfce xfce4-terminal"},{"alpine","faenza-icon-theme xfce4-whiskermenu-plugin xfce4 xfce4-terminal"},{"arch","xfce4 xfce4-terminal xfce4-goodies"}}
                },
                {
                    "xfce-lite", "Xfce Lite", "", "xfce4-session", "startxfce4", "lightdm", false, false,
                    "xfce4 xfce4-terminal xfce4-panel thunar", "精简安装,未美化",
                    {{"redhat","@xfce"},{"gentoo","xfce4-meta x11-terms/xfce4-terminal"},{"suse","patterns-xfce-xfce"},{"arch","xfce4 xfce4-terminal xfce4-panel thunar"}}
                },
                {
                    "lxqt", "🐦 LXQt", "🐦", "lxqt-session", "startlxqt", "sddm", false, false, "lxqt lxqt-qtplugin",
                    "LXDE团队基于Qt开发",
                    {{"redhat","@lxqt"},{"gentoo","lxqt-base/lxqt-meta"},
                     {"suse","patterns-lxqt-lxqt"},{"arch","lxqt xorg"},
                     {"alpine","openbox pcmfm rxvt-unicode tint2"}}
                },
                {
                    "lxde", "🕊 LXDE", "🕊", "lxsession", "startlxde", "lightdm", false, false, "lxde lxde-common",
                    "轻量化桌面,资源占用低",
                    {{"gentoo","media-fonts/wqy-bitmapfont lxde-base/lxde-meta"},{"alpine","lxsession"},
                     {"redhat","@lxde-desktop"},{"suse","patterns-lxde-lxde"},{"arch","lxde"}}
                },
                {
                    "mate", "🌿 MATE", "🌿", "mate-session", "mate-panel", "lightdm", false, false,
                    "mate-desktop-environment", "GNOME2的延续,舒适体验",
                    {{"arch","mate mate-extra"},{"redhat","@mate-desktop"},
                     {"gentoo","mate-base/mate-desktop mate-base/mate"},
                     {"suse","patterns-mate-mate"},{"alpine","mate-desktop-environment"}}
                },
                {
                    "kde", "🦖 KDE Plasma", "🦖", "startplasma-x11", "startkde", "sddm", false, false, true,
                    "kde-plasma-desktop", "风格华丽,功能丰富",
                    {{"arch","plasma-desktop dolphin konsole discover"},{"redhat","@KDE"},
                     {"gentoo","plasma-desktop plasma-nm plasma-pa sddm konsole"},
                     {"suse","-t pattern kde kde_plasma"},{"alpine","plasma-desktop breeze breeze-icons konsole discover"}}
                },
                {
                    "cinnamon", "🌲 Cinnamon", "🌲", "cinnamon-session", "cinnamon", "lightdm", true, false, true,
                    "cinnamon-desktop-environment", "基于GNOME,对用户友好",
                    {{"arch","cinnamon-translations cinnamon"},{"redhat","@Cinnamon Desktop"},
                     {"gentoo","gnome-extra/cinnamon gnome-extra/cinnamon-desktop gnome-extra/cinnamon-translations"},
                     {"suse","cinnamon cinnamon-control-center"},{"alpine","adapta-cinnamon"}}
                },
                {
                    "gnome", "👣 GNOME", "👣", "gnome-session", "gnome-shell-x11", "gdm", true, false, true,
                    "gnome-session gnome-shell", "GNU网络对象模型环境",
                    {{"arch","gnome-tweaks gnome"},{"redhat","@GNOME"},
                     {"gentoo","gnome-shell gdm gnome-terminal"},
                     {"suse","patterns-gnome-gnome_x11"},{"alpine","gnome-session gnome-shell"}}
                },
                {
                    "budgie", "🦜 Budgie", "🦜", "budgie-desktop", "budgie-panel", "lightdm", true, false, true,
                    "budgie-desktop", "虎皮鹦鹉,基于GNOME",
                    {{"arch","budgie-desktop"},{"void","budgie-desktop"}}
                },
                {
                    "dde", "🐋 Deepin DDE", "🐋", "startdde", "dde-launcher", "lightdm", true, false, true, "dde dde-desktop",
                    "国产精美桌面",
                    {{"arch","deepin xorg deepin-extra lightdm lightdm-deepin-greeter"}}
                },
                {
                    "deepin", "🐳 Deepin", "🐳", "deepin-session", "deepin-launcher", "lightdm", true, false, true,
                    "deepin-desktop-environment deepin-terminal", "Deepin完整桌面",
                    {{"arch","deepin xorg deepin-extra lightdm lightdm-deepin-greeter"},{"redhat","deepin-desktop"}}
                },
                {
                    "ukui", "🐱 UKUI", "🐱", "ukui-session", "ukui-panel", "lightdm", true, false, true,
                    "ukui-desktop-environment", "优麒麟桌面,简洁流畅",
                    {{"arch","ukui"}}
                },
                {
                    "cutefish", "🐟 Cutefish", "🐟", "cutefish-session", "cutefish-launcher", "lightdm", true, false,
                    "cutefish", "简洁美观的现代化桌面",
                    {{"arch","cutefish cutefish-core"}}
                },
                // ── 窗口管理器 ──
                {"icewm", "❄ IceWM", "", "icewm-session", "icewm", "", false, true, "icewm icewm-common", "意在提升感观和体验"},
                {
                    "openbox", "📦 Openbox", "", "openbox-session", "openbox", "", false, true, "openbox obconf obmenu",
                    "快速,轻巧,可扩展", {{"debian", "openbox openbox-menu"}}
                },
                {
                    "fvwm", "🪶 FVWM", "", "fvwm", "fvwm", "", false, true, "fvwm fvwm-icons", "强大的ICCCM2兼容WM",
                    {{"debian", "fvwm fvwm-icons"}}
                },
                {
                    "awesome", "🚀 Awesome", "", "awesome", "awesome", "", false, true, "awesome", "平铺式WM",
                    {{"debian", "awesome awesome-extra"}}
                },
                {
                    "enlightenment", "💡 Enlightenment", "", "enlightenment", "enlightenment", "", false, true,
                    "enlightenment", "X11 WM based on EFL"
                },
                {
                    "fluxbox", "📐 Fluxbox", "", "fluxbox", "fluxbox", "", false, true, "fluxbox", "高度可配置,低资源占用",
                    {{"debian", "bbmail bbpager bbtime fbpager fluxbox"}}
                },
                {
                    "i3", "🧩 i3", "", "i3", "i3", "", false, true, "i3 i3status i3lock dmenu", "改进的动态平铺WM",
                    {{"debian", "i3 i3-wm i3blocks"}, {"alpine", "i3wm"}}
                },
                {
                    "xmonad", "λ XMonad", "", "xmonad", "xmonad", "", false, true, "xmonad xmobar", "基于Haskell的平铺式WM",
                    {{"debian", "xmobar dmenu xmonad"}}
                },
                {"9wm", "9️⃣ 9wm", "", "9wm", "9wm", "", false, true, "9wm", "inspired by Plan 9's rio"},
                {"metacity", "🪟 Metacity", "", "metacity", "metacity", "", false, true, "metacity", "轻量的GTK+ WM"},
                {"twm", "📋 TWM", "", "twm", "twm", "", false, true, "twm", "Tab Window Manager"},
                {"aewm", "🧱 aewm", "", "aewm", "aewm", "", false, true, "aewm", "极简主义WM for X11"},
                {"aewm++", "🧱 aewm++", "", "aewm++", "aewm++", "", false, true, "aewm++", "最小WM written in C++"},
                {
                    "afterstep", "🪜 AfterStep", "", "afterstep", "afterstep", "", false, true, "afterstep",
                    "NEXTSTEP风格的WM"
                },
                {
                    "blackbox", "⬛ Blackbox", "", "blackbox", "blackbox", "", false, true, "blackbox", "WM for X",
                    {{"debian", "bbmail bbpager bbtime blackbox"}}
                },
                {"dwm", "🪶 dwm", "", "dwm", "dwm", "", false, true, "dwm st", "dynamic window manager"},
                {"mutter", "🪟 Mutter", "", "mutter", "mutter", "", false, true, "mutter", "轻量的GTK+ WM"},
                {
                    "bspwm", "🌲 bspwm", "", "bspwm", "bspwm", "", false, true, "bspwm sxhkd",
                    "Binary space partitioning WM"
                },
                {"clfswm", "💻 CLFSWM", "", "clfswm", "clfswm", "", false, true, "clfswm", "Common Lisp FullScreen WM"},
                {"ctwm", "📑 CTWM", "", "ctwm", "ctwm", "", false, true, "ctwm", "Claude's Tab WM"},
                {"evilwm", "👿 evilwm", "", "evilwm", "evilwm", "", false, true, "evilwm", "极简主义WM for X11"},
                {"flwm", "💨 FLWM", "", "flwm", "flwm", "", false, true, "flwm", "Fast Light WM"},
                {
                    "herbstluftwm", "🌳 herbstluftwm", "", "herbstluftwm", "herbstluftwm", "", false, true,
                    "herbstluftwm", "manual tiling WM for X11"
                },
                {"jwm", "🫙 JWM", "", "jwm", "jwm", "", false, true, "jwm", "very small & pure轻量纯净"},
                {
                    "kwin", "🪟 KWin", "", "kwin_x11", "kwin_x11", "", false, true, "kwin-x11", "KDE默认WM,X11 version",
                    {{"alpine", "kwin"}}
                },
                {"lwm", "🪶 LWM", "", "lwm", "lwm", "", false, true, "lwm", "轻量化WM"},
                {"marco", "🪟 Marco", "", "marco", "marco", "", false, true, "marco", "轻量化GTK+ WM for MATE"},
                {
                    "matchbox", "📱 Matchbox", "", "matchbox-window-manager", "matchbox-window-manager", "", false,
                    true, "matchbox-window-manager", "低配机福音",
                    {{"debian", "matchbox-themes-extra matchbox-window-manager"}}
                },
                {"miwm", "🧱 MIWM", "", "miwm", "miwm", "", false, true, "miwm", "microscopic WM"},
                {
                    "muffin", "🧁 Muffin", "", "muffin", "muffin", "", false, true, "muffin", "Cinnamon默认WM",
                    {{"debian", "murrine-themes muffin"}}
                },
                {"mwm", "🧱 MWM", "", "mwm", "mwm", "", false, true, "mwm", "Motif Window Manager"},
                {"oroborus", "🪟 Oroborus", "", "oroborus", "oroborus", "", false, true, "oroborus", "轻量WM for X11"},
                {
                    "pekwm", "🧱 PekWM", "", "pekwm", "pekwm", "", false, true, "pekwm", "轻量WM",
                    {{"debian", "pekwm-themes pekwm"}}
                },
                {"ratpoison", "🐀 Ratpoison", "", "ratpoison", "ratpoison", "", false, true, "ratpoison", "无鼠平铺WM"},
                {"sapphire", "💎 Sapphire", "", "sapphire", "sapphire", "", false, true, "sapphire", "最小的X11 WM之一"},
                {
                    "sawfish", "🐟 Sawfish", "", "sawfish", "sawfish", "", false, true, "sawfish", "可扩展WM",
                    {{"debian", "sawfish-themes sawfish"}}
                },
                {"spectrwm", "📐 SpectrWM", "", "spectrwm", "spectrwm", "", false, true, "spectrwm", "平铺式WM"},
                {"stumpwm", "🪵 StumpWM", "", "stumpwm", "stumpwm", "", false, true, "stumpwm", "Common Lisp平铺WM"},
                {"subtle", "🧩 Subtle", "", "subtle", "subtle", "", false, true, "subtle", "平铺式WM"},
                {"sugar", "🍬 Sugar", "", "sugar-session", "sugar-session", "", false, true, "sucrose", "Sugar学习平台"},
                {"tinywm", "🔬 TinyWM", "", "tinywm", "tinywm", "", false, true, "tinywm", "极简教学WM"},
                {"ukwm", "🪟 UKWM", "", "ukwm", "ukwm", "", false, true, "ukwm", "UKUI默认WM"},
                {"vdesk", "🖥 VDesk", "", "vdesk", "vdesk", "", false, true, "vdesk", "虚拟桌面WM"},
                {"vtwm", "📺 VTWM", "", "vtwm", "vtwm", "", false, true, "vtwm", "Virtual Tab WM"},
                {"w9wm", "9️⃣ w9wm", "", "w9wm", "w9wm", "", false, true, "w9wm", "Plan 9 inspired WM"},
                {"wm2", "🪟 WM2", "", "wm2", "wm2", "", false, true, "wm2", "最小X11 WM"},
                {"wmaker", "🪟 Window Maker", "", "wmaker", "wmaker", "", false, true, "wmaker", "NeXTSTEP风格WM"},
                {"wmii", "🪟 wmii", "", "wmii", "wmii", "", false, true, "wmii", "轻量平铺WM"},
                {"xfwm4", "🪟 Xfwm4", "", "xfwm4", "xfwm4", "", false, true, "xfwm4", "Xfce默认WM,可独立运行"},
                {
                    "exwm", "🧙 Emacs EXWM", "", "emacs-gtk", "emacs-gtk", "", false, true, "elpa-exwm emacs-gtk",
                    "Emacs X Window Manager"
                },
            };
            return reg;
        }
    } // namespace tmoe::domain::gui_config
} // namespace tmoe::domain
