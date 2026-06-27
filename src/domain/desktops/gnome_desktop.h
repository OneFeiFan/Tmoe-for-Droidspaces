#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

struct GnomeDesktop : DesktopBase {
    explicit GnomeDesktop(const TmoeConfig& c) : DesktopBase(c), info_(gui_config::all_desktops()[7]) {}
    std::string get_id() const override { return info_.id; }
    const DesktopInfo& get_info() const override { return info_; }
    bool recommends_tiger_vnc() const override { return true; }
    void will_be_installed_message() const override { Logger::info("GNOME: gnome-session / gnome-shell-x11"); }
    SessionCmds get_session_commands() const override;
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    const DesktopInfo& info_;
    std::string session_ = "1";
    void write_session_scripts();
};

} // namespace tmoe::domain
