#include "cinnamon_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "core/str_utils.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

PreInstallChoices CinnamonDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a || f != DistroFamily::Debian) return c;
    auto issue = SystemHelper::read_file("/etc/issue");
    if (contains(issue, "Linux Mint")) { c.pkg_list = "mint-meta-cinnamon mint-meta-core mint-artwork"; return c; }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"Lite or standard\" --yes-button \"lite\" --no-button \"standard\" --yesno 'lite/standard?' 0 0");
    if (r.exit_code == 0) { c.use_no_recommends = true; c.pkg_list = "cinnamon-l10n cinnamon"; }
    else c.pkg_list = "cinnamon-l10n cinnamon-desktop-environment cinnamon";
    return c;
}
void CinnamonDesktop::will_be_installed_message() const {
    Logger::info("Cinnamon: cinnamon-session / cinnamon");
    Logger::info(_("gui.cinnamon.package_list"));
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown)
        family = PackageManager::detect_distro_family();
    if (family == DistroFamily::Debian && Executor::has("apt-cache"))
        Executor::passthrough("apt-cache depends cinnamon-desktop-environment 2>/dev/null | head -20 || true");
}

void CinnamonDesktop::post_install_config(const PostInstallContext& ctx) {
    if (!cinnamon_warning()) return;
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) { desktop_utils::install_noto_fonts(ctx.family, true); }
    else if (ctx.family == DistroFamily::RedHat) Executor::passthrough(cfg_.install_command + " @'Cinnamon Desktop' 2>/dev/null || true");
    else if (ctx.family == DistroFamily::Arch) PackageManager::install({"cinnamon-translations","cinnamon"}, ctx.family);
    desktop_utils::install_language_packs(cfg_);
}

void CinnamonDesktop::post_install_extras(const PostInstallContext& ctx) {
    // Cinnamon 主题和壁纸
    auto family = ctx.family;
    // Linux Mint 专有壁纸
    auto issue = SystemHelper::read_file("/etc/issue");
    if (contains(issue, "Linux Mint")) {
        PackageManager::install({"mint-backgrounds-*", "mint-themes"}, family);
    }
    // 通用 GTK 主题
    if (family != DistroFamily::Alpine) {
        PackageManager::install({"arc-theme", "papirus-icon-theme"}, family);
    }
}

bool CinnamonDesktop::cinnamon_warning() const {
    Logger::info("------------------------");
    Logger::info(_("gui.cinnamon.warning.continue"));
    if (!Logger::confirm_yes_default(_("gui.cinnamon.warning.continue"))) {
        Logger::warn(_("gui.cinnamon.warning.cancelled"));
        return false;
    }
    return true;
}

} // namespace tmoe::domain
