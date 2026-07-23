#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "../gui_config/templates.h"
#include <filesystem>
#include "core/str_utils.h"
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace tmoe::domain::desktop_utils {
    void install_noto_fonts(DistroFamily family, bool is_debian) {
        if (!is_debian) return;
        PackageManager::install({"fonts-noto-cjk", "fonts-noto-color-emoji"}, family);
    }

    void install_language_packs(const TmoeConfig &cfg) {
        if (cfg.sub_distro != "ubuntu" && cfg.linux_distro != "debian") return;
        auto lang = std::string(I18n::current_lang());
        if (lang.find("zh") != 0) return;

        auto family = infer_family_from_config(cfg.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        // Quick path: zh_CN base language packs
        std::vector<std::string> pkgs = {"language-pack-zh-hans"};
        if (Executor::has("startplasma-x11") || Executor::has("plasmashell"))
            pkgs.emplace_back("language-pack-kde-zh-hans");
        if (Executor::has("gnome-shell") || Executor::has("gnome-session"))
            pkgs.emplace_back("language-pack-gnome-zh-hans");

        // Install language-selector-common to get check-language-support
        PackageManager::install("language-selector-common", family);

        // Run check-language-support to find all missing language packs
        auto result = Executor::shell("check-language-support 2>/dev/null");
        if (result.ok() && !result.stdout_data.empty()) {
            std::istringstream iss(result.stdout_data);
            std::string pkg;
            while (iss >> pkg) {
                pkgs.push_back(pkg);
            }
        }

        PackageManager::install(pkgs, family);
    }

    void purge_libfprint_and_clean(bool is_proot, bool is_debian) {
        if (!is_proot || !is_debian) return;
        Executor::passthrough("sudo apt purge -y ^libfprint 2>/dev/null || true");
        Executor::passthrough("sudo apt clean 2>/dev/null || true");
        Executor::passthrough("sudo apt autoclean 2>/dev/null || true");
    }

    void dpkg_configure_and_keyboard(bool is_debian) {
        if (!is_debian) return;
        Executor::passthrough(
                "printf 'debconf debconf/frontend select Noninteractive' | sudo debconf-set-selections 2>/dev/null || true");
        Executor::passthrough(
                "printf 'keyboard-configuration keyboard-configuration/layout select English (US)' | sudo debconf-set-selections 2>/dev/null || true");
        Executor::passthrough(
                "echo keyboard-configuration keyboard-configuration/layoutcode select us | sudo debconf-set-selections 2>/dev/null || true");
        Executor::passthrough("sudo dpkg --configure -a 2>/dev/null || true");
    }

    void remove_udisks_gvfs_for_proot(
            std::string_view desktop_id, bool is_proot, bool is_debian) {
        if (!is_proot || !is_debian) return;
        std::string d(desktop_id);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (contains(d, "xfce") ||
            contains(d, "lxde") ||
            contains(d, "lxqt")) {
            Executor::passthrough(
                    "sudo apt purge -y --allow-change-held-packages "
                    "^udisks2 ^gvfs ^gvfs-backends ^gvfs-daemons 2>/dev/null || true");
        }
    }

    std::string resolve_distro_pkg_list(
            const DesktopInfo &info, DistroFamily family) {
        std::string key = PackageManager::family_key(family);
        if (!info.distro_pkgs.empty()) {
            auto it = info.distro_pkgs.find(key);
            if (it != info.distro_pkgs.end()) return it->second;
        }
        return info.pkg_group;
    }
// ═══════════════════════════════════════════════════════════════
// 主题/壁纸/光标 — 从 DesktopManager 提取供 DesktopBase 子类使用
// ═══════════════════════════════════════════════════════════════

    void check_update_icon_caches_sh() {
        if (!Executor::has("update-icon-caches")) {
            SystemHelper::write_file("/usr/local/bin/update-icon-caches",
                                     gui_config::UPDATE_ICON_CACHES_SCRIPT);
            CommandBuilder("sudo").add_arg("chmod").add_arg("a+rx")
                    .add_arg("/usr/local/bin/update-icon-caches").execute();
        }
    }

    void download_arch_breeze_adapta_cursor_theme() {
        if (fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) return;
        Logger::step(_("gui.breeze.cursor_download"));
        Executor::shell("mkdir -pv /tmp/.breeze_theme && cd /tmp/.breeze_theme && "
                        "curl -Lo index.html 'https://mirrors.bfsu.edu.cn/archlinuxcn/any/' 2>/dev/null && "
                        "GREP_NAME='breeze-adapta-cursor-theme-git' && "
                        "LATEST=$(cat index.html | grep \"$GREP_NAME\" | grep '.pkg.tar.zst' | tail -n1 | "
                        "cut -d '=' -f3 | cut -d '\"' -f2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo data.tar.zst \"https://mirrors.bfsu.edu.cn/archlinuxcn/any/$LATEST\" 2>/dev/null && "
                        "tar --use-compress-program zstd -xvf data.tar.zst 2>/dev/null && "
                        "sudo cp -rf usr / 2>/dev/null || true");
        Executor::shell("rm -rf /tmp/.breeze_theme 2>/dev/null || true");
    }

    void install_breeze_theme_ext(const TmoeConfig &cfg) {
        Logger::step(_("gui.breeze.install"));
        download_arch_breeze_adapta_cursor_theme();
        auto family = infer_family_from_config(cfg.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Arch) {
            PackageManager::install({"breeze-icons", "breeze-gtk", "xfwm4-theme-breeze", "capitaine-cursors"},
                                    DistroFamily::Arch);
        } else {
            PackageManager::install({"breeze-icon-theme", "breeze-cursor-theme",
                                     "breeze-gtk-theme", "xfwm4-theme-breeze"}, family);
        }
        Logger::ok(_("gui.breeze.ok"));
    }

    void download_ubuntu_mate_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_mate"));
        SystemHelper::fetch_latest_and_extract(
                "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-mate-artwork/",
                "ubuntu-mate-wallpapers-photos.*all\\.deb", "umate_wp");
    }

    void download_kali_themes_common() {
        check_update_icon_caches_sh();
        // 主源 → 回退源
        if (!SystemHelper::fetch_latest_and_extract(
                "https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-themes/",
                "kali-themes-common.*all\\.deb", "kali_themes"))
            SystemHelper::fetch_latest_and_extract(
                    "http://http.kali.org/kali/pool/main/k/kali-themes/",
                    "kali-themes-common.*all\\.deb", "kali_themes_fb");
        Executor::shell(
                "sudo update-icon-caches /usr/share/icons/Flat-Remix-Blue-Dark "
                "/usr/share/icons/Flat-Remix-Blue-Light "
                "/usr/share/icons/desktop-base 2>/dev/null &");
        // Set Breeze-Adapta-Cursor as default cursor theme
        Executor::shell(
                "dbus-launch xfconf-query -c xsettings -t string "
                "-np /Gtk/CursorThemeName -s \"Breeze-Adapta-Cursor\" 2>/dev/null || true");
    }

} // namespace tmoe::domain::desktop_utils
