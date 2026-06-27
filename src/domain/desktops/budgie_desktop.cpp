#include "budgie_desktop.h"
#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices BudgieDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a || f != DistroFamily::Debian) return c;
    if (cfg_.sub_distro == "ubuntu") { auto r = Executor::passthrough(cfg_.tui_bin + " --title \"budgie/ubuntu-budgie\" --yes-button \"budgie\" --no-button \"ubuntu-budgie\" --yesno 'budgie/ubuntu-budgie?' 0 0"); if (r.exit_code == 1) { c.pkg_list = "ubuntu-budgie-desktop"; return c; } }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"budgie-core/desktop\" --yes-button \"core\" --no-button \"desktop\" --yesno 'core/desktop?' 0 0");
    if (r.exit_code == 0) { c.use_no_recommends = true; c.pkg_list = "budgie-core"; } else c.pkg_list = "budgie-desktop";
    return c;
}
void BudgieDesktop::post_install_config(const PostInstallContext& ctx) {
    if (ctx.is_debian) {
        SystemHelper::write_file("/usr/local/bin/budgie-desktop-builtin", "#!/bin/sh\nbudgie-wm --x11 --replace &\nbudgie-panel --replace &\nwait\n");
        CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/budgie-desktop-builtin").execute();
        desktop_utils::install_noto_fonts(ctx.family, true);
    }
    desktop_utils::install_language_packs(cfg_);
}

} // namespace tmoe::domain
