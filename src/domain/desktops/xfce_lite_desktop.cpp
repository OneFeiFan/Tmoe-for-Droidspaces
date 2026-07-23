#include "xfce_lite_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "domain/system/package_manager.h"
#include <filesystem>

#include "core/i18n.h"

namespace tmoe::domain {

    XfceLiteDesktop::XfceLiteDesktop(const TmoeConfig &cfg)
            : XfceDesktop(cfg, gui_config::all_desktops()[1])  // xfce-lite is idx 1
    {}

    void XfceLiteDesktop::will_be_installed_message() const {
        // Bash: 显示 package info + do_you_want_to_continue (Y/N 确认精简安装)
        Logger::info("XFCE Lite: xfce4-session / startxfce4");
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Debian && Executor::has("apt-cache"))
            Executor::passthrough("apt-cache depends xfce4 2>/dev/null | head -20 || true");
        Logger::info(_("gui.xfce4.lite_confirm"));
    }

// xfce-lite: 跳过壁纸和 papirus 图标，其他和 xfce 一样
    void XfceLiteDesktop::post_install_config(const PostInstallContext &ctx) {
        // xfce-lite: 跳过 xfce_warning() whiptail 确认
        // 跳过 install_debian_extras（whiskermenu/taskmanager/gvfs 等 20+ 包）
        // 跳过 xfce_color_scheme / configure_xfce_settings
        // 只保留精简基础配置

        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        desktop_utils::remove_udisks_gvfs_for_proot(get_id(), ctx.is_proot, ctx.is_debian);

        if (ctx.is_debian) {
            desktop_utils::install_noto_fonts(ctx.family, true);
        }
        if (ctx.family == DistroFamily::RedHat) {
            Executor::passthrough("sudo rm -v /etc/xdg/autostart/xfce-polkit.desktop 2>/dev/null || true");
        }

        // qt5ct（lite 也需要，调用父类 protected 方法）
        setup_qt5ct_env();

        // proot: 移除电源管理/屏保
        if (ctx.is_proot) {
            PackageManager::remove("xfce4-power-manager", ctx.family);
            PackageManager::remove("xfce4-power-manager-data", ctx.family);
            PackageManager::remove("xfce4-power-manager-plugins", ctx.family);
            PackageManager::remove("xfce4-screensaver", ctx.family);
        }

        desktop_utils::install_language_packs(cfg_);
    }

    void XfceLiteDesktop::post_install_extras(const PostInstallContext &ctx) {
        // 仅保留光标主题
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
        // xfce-lite: 跳过 papirus 图标和壁纸 (bash 原版 xfce_lite 也不设)
    }

} // namespace tmoe::domain
