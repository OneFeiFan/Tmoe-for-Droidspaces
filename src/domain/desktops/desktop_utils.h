#pragma once
#include "desktop_base.h"
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain::desktop_utils {

// ═══════════════════════════════════════════════════════════════
// 多个桌面类共享的 post-install 工具函数
// ═══════════════════════════════════════════════════════════════

/** 安装 Noto CJK + Emoji 字体（Debian 系） */
void install_noto_fonts(DistroFamily family, bool is_debian);

/** 安装 Ubuntu/Debian 中文语言包 */
void install_language_packs(const TmoeConfig& cfg);

/** proot + debian: purge libfprint + apt clean */
void purge_libfprint_and_clean(bool is_proot, bool is_debian);

/** dpkg --configure -a + keyboard-configuration */
void dpkg_configure_and_keyboard(bool is_debian);

/** proot + debian + xfce/lxde/lxqt 系列: 移除 udisks2/gvfs */
void remove_udisks_gvfs_for_proot(
    std::string_view desktop_id, bool is_proot, bool is_debian);

/** 从 DesktopInfo 的 distro_pkgs 获取特定发行版的包列表 */
std::string resolve_distro_pkg_list(
    const DesktopInfo& info, DistroFamily family);

} // namespace tmoe::domain::desktop_utils
