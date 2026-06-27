#include "dde_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices DdeDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a || f != DistroFamily::Debian) return c;
    if (cfg_.sub_distro == "deepin") { c.pkg_list = "dde"; return c; }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"DDE/DDE-extras\" --yes-button \"dde\" --no-button \"dde-extras\" --yesno 'dde/extras?' 0 0");
    c.pkg_list = (r.exit_code == 0) ? "ubuntudde-dde deepin-terminal" : "ubuntudde-dde ubuntudde-dde-extras";
    return c;
}
void DdeDesktop::post_install_config(const PostInstallContext& ctx) {
    if (ctx.is_debian) {
        desktop_utils::install_noto_fonts(ctx.family, true);
        Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
    }
    desktop_utils::install_language_packs(cfg_);
}

} // namespace tmoe::domain
