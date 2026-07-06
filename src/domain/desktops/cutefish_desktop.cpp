#include "cutefish_desktop.h"

#include "desktop_utils.h"
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

void CutefishDesktop::post_install_extras(const PostInstallContext& ctx) {
    // Cutefish 是极简 DE，没有额外美化需求
    // 仅安装 fish-pi 基础配置工具（如果存在）
    auto family = ctx.family;
    if (family == DistroFamily::Arch) {
        PackageManager::install({"cutefish-icons", "cutefish-wallpapers"}, family);
    }
    // 其他发行版：Cutefish 包本身已包含默认主题，无需额外安装
}

} // namespace tmoe::domain
