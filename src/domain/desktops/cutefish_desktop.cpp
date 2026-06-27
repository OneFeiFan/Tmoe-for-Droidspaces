#include "cutefish_desktop.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

void CutefishDesktop::post_install_config(const PostInstallContext& ctx) {
    Logger::info(_("gui.cutefish.intro"));
    if (ctx.family == DistroFamily::Arch) PackageManager::install({"cutefish","cutefish-core"}, ctx.family);
}

} // namespace tmoe::domain
