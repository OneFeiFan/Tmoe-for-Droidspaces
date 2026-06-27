#include "ukui_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices UkuiDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a || f != DistroFamily::Debian) return c;
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"ukui/kylin\" --yes-button \"ukui\" --no-button \"kylin\" --yesno 'ukui/kylin?' 0 0");
    if (r.exit_code == 0) c.pkg_list = "ukui-session-manager ukui-menu ukui-control-center ukui-screensaver ukui-themes peony";
    else c.pkg_list = "ubuntukylin-desktop";
    return c;
}
void UkuiDesktop::post_install_config(const PostInstallContext& ctx) {
    if (cfg_.sub_distro == "ubuntu") desktop_utils::install_noto_fonts(ctx.family, true);
    desktop_utils::install_language_packs(cfg_);
}

} // namespace tmoe::domain
