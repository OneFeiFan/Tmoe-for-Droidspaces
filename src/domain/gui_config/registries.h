#pragma once
#include <string>
#include <map>
#include <vector>

namespace tmoe::domain::gui_config {
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
} // namespace tmoe::domain::gui_config
