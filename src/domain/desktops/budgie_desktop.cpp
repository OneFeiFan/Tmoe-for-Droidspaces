#include "budgie_desktop.h"
#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

SessionCmds BudgieDesktop::get_session_commands() const {
    return (session_ == "panel") ? SessionCmds{"budgie-desktop-builtin","budgie-panel"}
                                 : SessionCmds{"budgie-desktop","budgie-desktop-builtin"};
}
PreInstallChoices BudgieDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c;
    if (a) return c;
    // 检查不支持的发行版
    if (f != DistroFamily::Debian && f != DistroFamily::Arch && f != DistroFamily::Void_ && f != DistroFamily::Solus) {
        Logger::error(_("gui.budgie.unsupported_distro"));
        return c;
    }
    if (f != DistroFamily::Debian) return c;
    // bash: choose_budgie_panel_or_desktop — 问 session 类型（不影响包列表）
    auto sr = Executor::passthrough(cfg_.tui_bin + " --title \"budgie session\" --yes-button \"panel\" --no-button \"desktop\" --yesno 'panel/desktop?' 0 0");
    session_ = (sr.exit_code == 0) ? "panel" : "desktop";
    if (cfg_.sub_distro == "ubuntu") { auto r = Executor::passthrough(cfg_.tui_bin + " --title \"budgie/ubuntu-budgie\" --yes-button \"budgie\" --no-button \"ubuntu-budgie\" --yesno 'budgie/ubuntu-budgie?' 0 0"); if (r.exit_code == 1) { c.pkg_list = "ubuntu-budgie-desktop"; return c; } }
    auto r = Executor::passthrough(cfg_.tui_bin + " --title \"budgie-core/desktop\" --yes-button \"core\" --no-button \"desktop\" --yesno 'core/desktop?' 0 0");
    if (r.exit_code == 0) { c.use_no_recommends = true; c.pkg_list = "budgie-core"; } else c.pkg_list = "budgie-desktop";
    return c;
}
void BudgieDesktop::will_be_installed_message() const {
    Logger::info("Budgie: budgie-desktop / budgie-panel");
    Logger::info(_("gui.budgie.package_list"));
    // proot 警告移到安装前显示
    bool is_proot = (cfg_.is_termux || cfg_.linux_distro == "Android");
    if (is_proot) Logger::warn(_("gui.budgie.proot_warn"));
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown)
        family = PackageManager::detect_distro_family();
    if (family == DistroFamily::Debian && Executor::has("apt-cache"))
        Executor::passthrough("apt-cache depends budgie-desktop 2>/dev/null | head -20 || true");
}

void BudgieDesktop::post_install_config(const PostInstallContext& ctx) {
    if (!budgie_warning()) return;
    Logger::info("Budgie desktop — based on GNOME, elegant and modern");
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) {
        desktop_utils::install_noto_fonts(ctx.family, true);
    }
    // bash: cat_budgie_desktop_builtin_session — 全发行版创建
    SystemHelper::write_file("/usr/local/bin/budgie-desktop-builtin", "#!/bin/sh\nbudgie-wm --x11 --replace &\nbudgie-panel --replace &\nwait\n");
    CommandBuilder("sudo").add_arg("chmod").add_arg("a+rx").add_arg("/usr/local/bin/budgie-desktop-builtin").execute();
    desktop_utils::install_language_packs(cfg_);
}

void BudgieDesktop::post_install_extras(const PostInstallContext& ctx) {
    // Budgie 扩展和主题（Bash 原版在 Budgie 安装后安装 gnome-tweaks 等 GNOME 工具）
    auto family = ctx.family;
    // budgie 基于 GNOME，可与 GNOME tweaks 共用
    PackageManager::install({"gnome-tweaks", "budgie-extras"}, family);
    if (family == DistroFamily::Debian) {
        PackageManager::install({"arc-theme", "papirus-icon-theme"}, family);
    }
    // ubuntu-budgie 专有壁纸
    if (ctx.is_debian && ctx.is_ubuntu) {
        PackageManager::install("ubuntu-budgie-wallpapers", family);
    }
}

bool BudgieDesktop::budgie_warning() const {
    Logger::info("------------------------");
    Logger::info(_("gui.budgie.warning.continue"));
    if (!Logger::confirm_yes_default(_("gui.budgie.warning.continue"))) {
        Logger::warn(_("gui.budgie.warning.cancelled"));
        return false;
    }
    return true;
}

} // namespace tmoe::domain
