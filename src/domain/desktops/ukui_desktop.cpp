#include "ukui_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"
#include <cstdlib>

#include "domain/gui/desktop_manager.h"

namespace tmoe::domain {

void UkuiDesktop::will_be_installed_message() const {
    Logger::info("UKUI: ukui-session / ukui-panel");

    // Bash: tmoe_desktop_warning — proot 环境警告
    bool is_proot = cfg_.is_termux || cfg_.linux_distro == "Android";
    if (is_proot) {
        Logger::warn(_("gui.ukui.proot_warning"));
    }

    // Bash: 安装前包列表
    Logger::info(_("gui.ukui.package_list"));

    // Bash: tips_of_tiger_vnc_server — Tiger VNC 推荐
    Logger::info(_("gui.ukui.tiger_vnc_tip"));

    // Bash: do_you_want_to_continue — 终端 Y/N 确认
    if (!Logger::confirm_yes_default(_("gui.ukui.confirm_install"))) {
        std::exit(0);
    }
}

PreInstallChoices UkuiDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c;
    // bash: 仅支持 debian 和 arch
    if (f != DistroFamily::Debian && f != DistroFamily::Arch) {
        Logger::warn(_("gui.ukui.unsupported_distro"));
        Logger::press_enter();
        return c;
    }
    if (f == DistroFamily::Arch) return c; // registry has arch pkg
    // bash: auto 模式直接用 ubuntukylin-desktop
    if (a) { c.pkg_list = "ubuntukylin-desktop"; return c; }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"ukui/kylin\" --yes-button \"ukui\" --no-button \"kylin\" --yesno 'ukui/kylin?' 0 0");
    if (r.exit_code == 0) c.pkg_list = "ukui-session-manager ukui-menu ukui-control-center ukui-screensaver ukui-themes peony";
    else c.pkg_list = "ubuntukylin-desktop";
    return c;
}

void UkuiDesktop::post_install_config(const PostInstallContext& ctx) {
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) desktop_utils::install_noto_fonts(ctx.family, true);
    desktop_utils::install_language_packs(cfg_);
}

void UkuiDesktop::post_install_extras(const PostInstallContext& ctx) {
    // UKUI 主题和壁纸（Bash 原版在 UKUI 安装后安装主题包）
    auto family = ctx.family;
    if (family == DistroFamily::Debian) {
        PackageManager::install({"ukui-themes", "ukui-wallpapers", "ubuntukylin-wallpapers"}, family);
    }
    if (family == DistroFamily::Arch) {
        PackageManager::install({"ukui-themes", "ukui-wallpapers"}, family);
    }
}

} // namespace tmoe::domain
