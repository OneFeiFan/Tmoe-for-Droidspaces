#include "cutefish_desktop.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

void CutefishDesktop::post_install_config(const PostInstallContext& ctx) {
    Logger::info(_("gui.cutefish.intro"));
    if (ctx.is_proot) Logger::warn(_("gui.cutefish.systemd_warn"));
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) desktop_utils::install_noto_fonts(ctx.family, true);
    desktop_utils::install_language_packs(cfg_);
    if (ctx.family == DistroFamily::Arch) PackageManager::install({"cutefish","cutefish-core"}, ctx.family);
}

} // namespace tmoe::domain
