#include "mate_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/str_utils.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include "ui/dialog_helpers.h"

namespace tmoe::domain {

MateDesktop::MateDesktop(const TmoeConfig& cfg)
    : DesktopBase(cfg, gui_config::all_desktops()[4])  // mate is idx 4
{}

void MateDesktop::will_be_installed_message() const {
    Logger::info("MATE: mate-session / mate-panel");
    Logger::info(_("gui.mate.package_list"));
    // Bash: apt-cache show 包信息预览
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown)
        family = PackageManager::detect_distro_family();
    if (family == DistroFamily::Debian && Executor::has("apt-cache"))
        Executor::passthrough("apt-cache show mate-desktop-environment 2>/dev/null | head -20 || true");
}

// ── 阶段2: mate/ubuntu-mate + core/lite + Linux Mint ──
PreInstallChoices MateDesktop::pre_install_choices(
    DistroFamily family, bool is_auto_mode) {

    PreInstallChoices c;
    if (is_auto_mode || family != DistroFamily::Debian) return c;

    // Linux Mint 优先
    auto issue = SystemHelper::read_file("/etc/issue");
    if (contains(issue, "Linux Mint")) {
        c.pkg_list = "mint-meta-mate mint-meta-core mint-artwork";
        return c;
    }

    bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
    if (is_ubuntu) {
        if (ui::dialog::yesno(cfg_, "Mate or Ubuntu-MATE", "前者为普通mate,后者为ubuntu-mate", "mate", "ubuntu-mate") == 1) {
            c.pkg_list = "ubuntu-mate-desktop"; return c;
        }
    }

    // core / lite
    if (ui::dialog::yesno(cfg_, "MATE-CORE or MATE-LITE", "前者为普通mate,后者为精简版mate", "core", "lite") != 0) {
        c.use_no_recommends = true;
        c.pkg_list = "mate-session-manager mate-settings-daemon marco mate-terminal mate-panel";
    }
    return c;
}

// ── 阶段3: fonts + arch warning + apt clean ──
void MateDesktop::post_install_config(const PostInstallContext& ctx) {
    // Bash: do_you_want_to_continue 终端 Y/N 确认
    if (!Logger::confirm_yes_default(_("gui.mate.confirm_install"))) return;

    // Arch proot 警告: 终端 Y/N 确认 + 提示替代方案
    if (ctx.family == DistroFamily::Arch && ctx.is_proot) {
        Logger::warn("MATE may flicker in Arch proot.");
        Logger::info("Alternatives: try lxde, lxqt, or xfce instead.");
        if (!Logger::confirm_yes_default(_("gui.mate.arch_proot_warning")))
            return;
    }

    // 非 Debian 发行版时显示不支持的警告
    if (!ctx.is_debian && ctx.family == DistroFamily::Solus)
        Logger::warn("ERROR!未适配solus");

    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    // fonts + clean
    if (ctx.is_debian) {
        desktop_utils::install_noto_fonts(ctx.family, true);
        Executor::passthrough("sudo apt clean 2>/dev/null || true");
        Executor::passthrough("sudo apt autoclean 2>/dev/null || true");
    }
    desktop_utils::install_language_packs(cfg_);
}

void MateDesktop::post_install_extras(const PostInstallContext& /*ctx*/) {
    // 安装 ubuntu-mate 壁纸（Bash 原版在 MATE 安装后下载）
    desktop_utils::download_ubuntu_mate_wallpaper();
    // 安装 mate-tweak（MATE 配置工具）
    PackageManager::install("mate-tweak", DistroFamily::Debian);
}

} // namespace tmoe::domain
