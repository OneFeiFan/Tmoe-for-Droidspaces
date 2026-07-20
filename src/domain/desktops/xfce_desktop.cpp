#include "xfce_desktop.h"
#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include <filesystem>
#include "core/str_utils.h"
#include <sstream>
#include "ui/dialog_helpers.h"

namespace tmoe::domain {
    XfceDesktop::XfceDesktop(const TmoeConfig &cfg)
        : XfceDesktop(cfg, gui_config::all_desktops()[0]) // xfce is idx 0
    {
    }

    void XfceDesktop::will_be_installed_message() const {
        // Bash: 终端 printf 包信息 + 依赖预览
        Logger::info("XFCE: xfce4-session / startxfce4");
        Logger::info(_("gui.xfce4.package_list"));
        // Bash: apt-cache depends/pacman -Si/apk info 预览
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Debian && Executor::has("apt-cache"))
            Executor::passthrough("apt-cache depends xfce4 2>/dev/null | head -20 || true");
        else if (family == DistroFamily::Arch && Executor::has("pacman"))
            Executor::passthrough("pacman -Si xfdesktop 2>/dev/null || true");
        else if (Executor::has("apk"))
            Executor::passthrough("apk info xfce4 2>/dev/null || true");
    }

    // ── 阶段2: 装包前版本选择 ──
    PreInstallChoices XfceDesktop::pre_install_choices(
        DistroFamily family, bool is_auto_mode) {
        PreInstallChoices c;
        if (family == DistroFamily::Debian
            && cfg_.sub_distro == "ubuntu" && !is_auto_mode) {
            if (ui::dialog::yesno(cfg_, "Xfce or Xubuntu-desktop", "前者为普通xfce,后者为xubuntu", "xfce", "xubuntu") == 1)
                c.pkg_list = "xubuntu-desktop";
        }
        return c;
    }

    // ── 阶段3: 系统配置 (原 DesktopManager::post_install_xfce 全部内容) ──
    void XfceDesktop::post_install_config(const PostInstallContext &ctx) {
        if (!xfce_warning()) return;
        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        desktop_utils::remove_udisks_gvfs_for_proot(get_id(), ctx.is_proot, ctx.is_debian);

        // ── debian_xfce4_extras ──
        if (ctx.is_debian) {
            desktop_utils::install_noto_fonts(ctx.family, true);
            install_debian_extras(ctx);
        }
        if (ctx.family == DistroFamily::RedHat) {
            Executor::passthrough("sudo rm -v /etc/xdg/autostart/xfce-polkit.desktop 2>/dev/null || true");
        }
        // ── qt5ct 全发行版 ──
        setup_qt5ct_env();

        // ── Breeze 光标 ──
        if (ctx.is_debian || ctx.family == DistroFamily::Arch)
            download_breeze_cursor(ctx);

        // ── xfce4 config + 配色 + 主题 ──
        configure_xfce_settings(ctx);
        xfce_color_scheme();

        // ── proot: 移除电源管理/屏保 ──
        if (ctx.is_proot) {
            PackageManager::remove("xfce4-power-manager", ctx.family);
            PackageManager::remove("xfce4-power-manager-data", ctx.family);
            PackageManager::remove("xfce4-power-manager-plugins", ctx.family);
            PackageManager::remove("xfce4-screensaver", ctx.family);
        }
        desktop_utils::install_language_packs(cfg_);
    }

    // ── 阶段3b: 外观美化 ──
    void XfceDesktop::post_install_extras(const PostInstallContext &ctx) {
        // Breeze Adapta 光标主题
        if (ctx.family != DistroFamily::Alpine) {
            if (!fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) {
                Executor::shell(
                    "cd /tmp && "
                    "wget -qO breeze-adapta-cursor.tar.gz "
                    "'https://mirrors.bfsu.edu.cn/archlinux/community/os/x86_64/"
                    "breeze-adapta-cursor-theme-5.90.0-1-any.pkg.tar.zst' 2>/dev/null || "
                    "curl -sL 'https://gitee.com/ak2/breeze-adapta-cursor/raw/master/"
                    "breeze-adapta-cursor.tar.gz' -o breeze-adapta-cursor.tar.gz 2>/dev/null; "
                    "sudo tar -xzf breeze-adapta-cursor.tar.gz -C /usr/share/icons/ 2>/dev/null || true");
            }
        }
        // Papirus 图标主题
        if (ctx.family != DistroFamily::Alpine) {
            if (!fs::exists("/usr/share/icons/Papirus")) {
                PackageManager::install("papirus-icon-theme", ctx.family);
            }
        }
        // 面板: 已在 post_install_config→configure_xfce_settings 中处理
        // Wallpaper: 仅全量 xfce (非 xfce-lite) 才设壁纸
        {
            std::string home = SystemHelper::user_home();
            std::string wp_dir = SystemHelper::user_pictures_dir();
            fs::create_directories(wp_dir);
            Executor::shell(
                "cd /usr/share/backgrounds && "
                "sudo curl -sL 'https://gitee.com/ak2/icons/raw/master/debian-xfce.jpg' "
                "-o xfce-stripes.png 2>/dev/null || true");
        }
    }

    // ═══════════════════════════════════════════
    // private helpers
    // ═══════════════════════════════════════════

    bool XfceDesktop::xfce_warning() const {
        // Bash: 终端输出兼容性表格 + do_you_want_to_continue (终端 read Y/N)
        Logger::info(_("gui.xfce4.warning"));
        if (!Logger::confirm_yes_default(_("gui.xfce4.warning.continue"))) {
            Logger::warn(_("gui.xfce4.warning.cancelled"));
            return false;
        }
        return true;
    }

    void XfceDesktop::install_debian_extras(const PostInstallContext &ctx) {
        PackageManager::install({
                                    "xfce4-whiskermenu-plugin", "xfce4-taskmanager",
                                    "xfce4-places-plugin", "xfce4-netload-plugin", "xfce4-battery-plugin",
                                    "xfce4-datetime-plugin", "xfce4-verve-plugin", "xfce4-mount-plugin",
                                    "xfce4-screenshooter", "xfce4-clipman-plugin", "xfce4-pulseaudio-plugin",
                                    "thunar-archive-plugin", "gvfs", "gvfs-backends", "gvfs-fuse",
                                    "engrampa", "ristretto", "mousepad", "menulibre", "mugshot"
                                }, ctx.family);

        if (!fs::exists("/usr/share/themes/Breeze/xfwm4/themerc"))
            PackageManager::install("xfwm4-theme-breeze", ctx.family);

        if (cfg_.sub_distro == "kali") {
            if (!fs::exists("/usr/share/icons/Windows-10-Icons")) {
                Logger::step("Installing Kali Undercover theme...");
                Executor::shell(
                    "mkdir -pv /tmp/.kali-undercover-win10-theme && cd /tmp/.kali-undercover-win10-theme && "
                    "UNDERCOVERlatestLINK=\"$(curl -L 'https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-undercover/' 2>/dev/null | grep all.deb | tail -n1 | cut -d '=' -f3 | cut -d '\"' -f2)\" && "
                    "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s5 -x5 -k1M -o kali-undercover.deb 'https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-undercover/'\"$UNDERCOVERlatestLINK\" 2>/dev/null || "
                    "apt download kali-undercover 2>/dev/null && mv *deb kali-undercover.deb) && "
                    "ar xv kali-undercover.deb && cd / && sudo tar -Jxvf /tmp/.kali-undercover-win10-theme/data.tar.xz ./usr 2>/dev/null && "
                    "sudo mv -f /usr/bin/kali-undercover /usr/local/bin/ 2>/dev/null; update-icon-caches /usr/share/icons/Windows-10-Icons 2>/dev/null &");
            }
            PackageManager::install("kali-themes-common", DistroFamily::Debian);
            Executor::shell(
                "dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s Windows-10-Icons 2>/dev/null || true");
            // 下载 kali-themes-common 图标主题（Bash: git_clone_kali_themes_common）
            desktop_utils::download_kali_themes_common();
        }

        if (!Executor::has("xfce4-panel-profiles")) {
            PackageManager::install("xfce4-panel-profiles", ctx.family);
        }
    }

    void XfceDesktop::setup_qt5ct_env() {
        if (!Executor::has("qt5ct"))
            PackageManager::install("qt5ct", DistroFamily::Debian);

        if (Executor::has("qt5ct")) {
            if (!fs::exists("/etc/environment"))
                SystemHelper::write_file("/etc/environment", "");

            auto content = SystemHelper::read_file("/etc/environment");
            bool has_qt = false;
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                size_t ns = line.find_first_not_of(" \t");
                if (ns != std::string::npos && line[ns] == '#') continue;
                if (contains(line, "QT_QPA_PLATFORMTHEME=")) {
                    has_qt = true;
                    break;
                }
            }
            if (!has_qt)
                SystemHelper::append_file(fs::path("/etc/environment"),
                                          "export QT_QPA_PLATFORMTHEME=qt5ct\n");
        }
    }

    void XfceDesktop::download_breeze_cursor(const PostInstallContext & /*ctx*/) {
        if (fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) return;
        Executor::passthrough(
            "wget -qO /tmp/breeze-adapta-cursor.tar.gz "
            "'https://github.com/arch-linux-archive/community/raw/master/"
            "breeze-adapta-cursor-theme.tar.gz' 2>/dev/null && "
            "sudo tar -xzf /tmp/breeze-adapta-cursor.tar.gz -C /usr/share/icons/ 2>/dev/null || true");
    }

    void XfceDesktop::configure_xfce_settings(const PostInstallContext &ctx) {
        std::string home = SystemHelper::user_home();
        std::string xfce_conf = home + "/.config/xfce4/xfconf/xfce-perchannel-xml/";
        CommandBuilder("mkdir").add_flag("-p").add_arg(xfce_conf).add_raw("2>/dev/null").execute();

        if (!fs::exists(xfce_conf + "xfce4-desktop.xml"))
            Executor::shell("cp -f /etc/xdg/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml "
                            + xfce_conf + " 2>/dev/null || true");
        if (!fs::exists(xfce_conf + "xfce4-panel.xml"))
            Executor::shell("cp -f /etc/xdg/xfce4/panel/default.xml "
                            + xfce_conf + "xfce4-panel.xml 2>/dev/null || true");

        // 图标主题
        std::string icon_theme = (ctx.family == DistroFamily::Alpine)
                                     ? "Faenza"
                                     : "Flat-Remix-Blue-Light";
        Executor::shell("dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s "
                        + icon_theme + " 2>/dev/null || true");
        // 光标主题
        Executor::shell("dbus-launch xfconf-query -c xsettings -t string "
            "-np /Gtk/CursorThemeName -s \"Breeze-Adapta-Cursor\" 2>/dev/null || true");
        // 工作区数量
        Executor::shell("dbus-launch xfconf-query -c xfwm4 -t int "
            "-np /general/workspace_count -s 2 2>/dev/null || true");
        // GTK 主题
        Executor::shell("dbus-launch xfconf-query -c xsettings -np /Net/ThemeName "
            "-s \"Adwaita-dark\" 2>/dev/null || "
            "xfconf-query -c xsettings -np /Net/ThemeName -s \"Greybird\" 2>/dev/null || true");
    }

    void XfceDesktop::xfce_color_scheme() const {
        if (!fs::exists("/usr/share/xfce4/terminal/colorschemes/Monokai Remastered.theme")) {
            Logger::info(_("gui.xfce4.color_scheme_config"));
            Executor::shell("cd /usr/share/xfce4/terminal && "
                "sudo curl -Lo 'colorschemes.tar.xz' "
                "'https://gitee.com/mo2/xfce-themes/raw/terminal/colorschemes.tar.xz' 2>/dev/null && "
                "sudo tar -Jxvf 'colorschemes.tar.xz' 2>/dev/null || true");
        }

        std::string home = SystemHelper::user_home();
        std::string termrc = home + "/.config/xfce4/terminal/terminalrc";

        auto content = SystemHelper::read_file(termrc);
        bool has_palette = (contains(content, "ColorPalette="));
        if (!has_palette) {
            // 移除旧配色行，写入新配色
            std::string filtered;
            for (auto& line : split(content, '\n')) {
                if (line.find("ColorPalette=") == std::string::npos &&
                    line.find("ColorForeground=") == std::string::npos &&
                    line.find("ColorBackground=") == std::string::npos)
                    filtered += line + "\n";
            }
            SystemHelper::write_file(fs::path(termrc), filtered);
            SystemHelper::append_file(fs::path(termrc),
                                      "ColorPalette=#000000;#ff3333;#b8cc52;#e7c547;#36a3d9;#f07178;#95e6cb;#ffffff;"
                                      "#323232;#ff6565;#eafe84;#fff779;#68d5ff;#ffa3aa;#c7fffd;#ffffff\n"
                                      "ColorForeground=#e6e1cf\nColorBackground=#0f1419\n");
        }

        bool has_font = (contains(SystemHelper::read_file(termrc), "FontName"));
        if (!has_font) {
            if (fs::exists("/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf"))
                SystemHelper::append_file(fs::path(termrc), "FontName=Iosevka Term Bold 12\n");
            else if (fs::exists("/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc"))
                SystemHelper::append_file(fs::path(termrc), "FontName=Noto Sans Mono CJK SC Bold 12\n");
        }
    }
} // namespace tmoe::domain
