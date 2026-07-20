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

/** 标准 post_install_config 管线：键盘配置 + libfprint 清理 + Noto 字体 + 语言包。
 *  在 13 个桌面类的 post_install_config() 中完全重复，提取为公共函数。 */
inline void standard_config(const PostInstallContext& ctx, const TmoeConfig& cfg) {
    dpkg_configure_and_keyboard(ctx.is_debian);
    purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) install_noto_fonts(ctx.family, true);
    install_language_packs(cfg);
}

// ═══════════════════════════════════════════════════════════════
// 主题/壁纸/光标 — 从 DesktopManager 提取供 DesktopBase 子类使用
// ═══════════════════════════════════════════════════════════════

/** 确保 update-icon-caches 脚本存在 */
void check_update_icon_caches_sh();

/** 下载 Arch Breeze-Adapta-Cursor 光标主题 */
void download_arch_breeze_adapta_cursor_theme();

/** 安装 Breeze 全家桶（光标 + 图标 + GTK + xfwm4 主题） */
void install_breeze_theme_ext(const TmoeConfig& cfg);

/** 下载 ubuntu-mate 壁纸 */
void download_ubuntu_mate_wallpaper();

/** 下载并安装 kali-themes-common（图标主题 + 光标主题） */
void download_kali_themes_common();

} // namespace tmoe::domain::desktop_utils
