#include "ukui_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices UkuiDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c;
    // bash: 仅支持 debian 和 arch
    if (f != DistroFamily::Debian && f != DistroFamily::Arch) {
        Logger::warn("UKUI is only supported on Debian/Ubuntu and Arch Linux");
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
