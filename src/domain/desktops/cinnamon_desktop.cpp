#include "cinnamon_desktop.h"
#include "desktop_utils.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices CinnamonDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a || f != DistroFamily::Debian) return c;
    auto issue = SystemHelper::read_file("/etc/issue");
    if (issue.find("Linux Mint") != std::string::npos) { c.pkg_list = "mint-meta-cinnamon mint-meta-core mint-artwork"; return c; }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"Lite or standard\" --yes-button \"lite\" --no-button \"standard\" --yesno 'lite/standard?' 0 0");
    if (r.exit_code == 0) { c.use_no_recommends = true; c.pkg_list = "cinnamon-l10n cinnamon"; }
    else c.pkg_list = "cinnamon-l10n cinnamon-desktop-environment cinnamon";
    return c;
}
void CinnamonDesktop::post_install_config(const PostInstallContext& ctx) {
    if (ctx.is_debian) { desktop_utils::install_noto_fonts(ctx.family, true); Executor::passthrough("dpkg --configure -a 2>/dev/null || true"); }
    else if (ctx.family == DistroFamily::RedHat) Executor::passthrough(cfg_.install_command + " @'Cinnamon Desktop' 2>/dev/null || true");
    else if (ctx.family == DistroFamily::Arch) PackageManager::install({"cinnamon-translations","cinnamon"}, ctx.family);
    desktop_utils::install_language_packs(cfg_);
}

} // namespace tmoe::domain
