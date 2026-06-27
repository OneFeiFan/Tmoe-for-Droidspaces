#include "gnome_desktop.h"
#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

SessionCmds GnomeDesktop::get_session_commands() const {
    if (session_ == "2") return {"gnome-flashback-metacity","gnome-shell-x11"};
    if (session_ == "4") return {"gnome-session-ubuntu","gnome-session"};
    if (session_ == "5") return {"gnome-session-classic","gnome-session"};
    return {"gnome-shell-x11","gnome-flashback-metacity"};
}
PreInstallChoices GnomeDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a) return c;
    if (f == DistroFamily::Debian) {
        session_ = Executor::tui_select(cfg_.tui_bin +
            " --title \"gnome-session\" --menu \"Select GNOME session\" 0 0 0 "
            "\"1\" \"gnome-shell-x11\" \"2\" \"gnome-flashback\" "
            "\"3\" \"gnome-session\" \"4\" \"gnome-session-ubuntu\" "
            "\"5\" \"gnome-session-classic\" \"0\" \"Back\"");
        if (session_ == "2") { c.pkg_list = "gnome-panel gnome-menus gnome-shell gnome-session-flashback gnome-session"; return c; }
        if (session_.empty() || session_ == "0") return c;
        if (cfg_.sub_distro == "ubuntu") { auto r = Executor::passthrough(cfg_.tui_bin + " --title \"gnome/ubuntu\" --yes-button \"gnome\" --no-button \"ubuntu-desktop\" --yesno 'gnome/ubuntu-desktop?' 0 0"); if (r.exit_code == 1) { c.pkg_list = "ubuntu-desktop"; return c; } }
        if (c.pkg_list.empty()) { auto r = Executor::passthrough(cfg_.tui_bin + " --title \"gnome-shell/core\" --yes-button \"gnome-shell\" --no-button \"gnome-core\" --yesno 'shell/core?' 0 0"); c.pkg_list = (r.exit_code == 0) ? "xorg gnome-panel gnome-menus gnome-shell gnome-session" : "gnome-core"; }
    } else if (f == DistroFamily::Arch) {
        auto r = Executor::passthrough(cfg_.tui_bin + " --title \"gnome/gnome-extra\" --yes-button \"gnome\" --no-button \"gnome-extra\" --yesno 'gnome/extra?' 0 0");
        c.pkg_list = (r.exit_code == 0) ? "gnome-tweaks gnome" : "gnome-extra gnome";
    } else if (f == DistroFamily::RedHat) {
        c.pkg_list = "@GNOME";
    } return c;
}
void GnomeDesktop::post_install_config(const PostInstallContext& ctx) {
    Logger::info("GNOME desktop install");
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    if (ctx.is_debian) { desktop_utils::install_noto_fonts(ctx.family, true); write_session_scripts(); }
    desktop_utils::install_language_packs(cfg_);
}
void GnomeDesktop::write_session_scripts() {
    auto wr = [&](const char* n, const std::string& c) { SystemHelper::write_file("/usr/local/bin/"+std::string(n),c); CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/"+std::string(n)).execute(); };
    // bash: gnome-shell-x11 总是创建（不管选哪个 session）
    wr("gnome-shell-x11", "#!/bin/sh\nexec gnome-shell --x11 \"$@\"\n");
    if (session_ == "2") {
        // bash: ln_s_gnome_flashback_metacity — 检测 --builtin 支持，不支持则退回 metacity &
        std::string fb = "#!/bin/sh\nexport XDG_CURRENT_DESKTOP=\"GNOME-Flashback:GNOME\"\n"
            "if gnome-session --help 2>&1 | grep -q builtin; then\n"
            "  exec gnome-session --builtin --session=gnome-flashback-metacity --disable-acceleration-check \"$@\"\n"
            "else\n  metacity &\n  exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check \"$@\"\nfi\n";
        wr("gnome-flashback-metacity", fb);
    }
    if (session_ == "4") wr("gnome-session-ubuntu", "#!/bin/sh\nexport DESKTOP_SESSION=ubuntu\nexport GNOME_SHELL_SESSION_MODE=ubuntu\nexport XDG_CONFIG_DIRS=/etc/xdg/xdg-ubuntu:/etc/xdg\nexec gnome-session --session=ubuntu --disable-acceleration-check \"$@\"\n");
    if (session_ == "5") wr("gnome-session-classic", "#!/bin/sh\nexport GNOME_SHELL_SESSION_MODE=classic\nexec gnome-session --session=gnome-classic --disable-acceleration-check \"$@\"\n");
}

} // namespace tmoe::domain
