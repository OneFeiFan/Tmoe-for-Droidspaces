#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

struct CinnamonDesktop : DesktopBase {
    explicit CinnamonDesktop(const TmoeConfig& c) : DesktopBase(c, gui_config::all_desktops()[6]) {}
    void will_be_installed_message() const override;
    SessionCmds get_session_commands() const override {
        // bash: proot 里 session 命令对调
        bool is_proot = (cfg_.is_termux || cfg_.linux_distro == "Android");
        return is_proot ? SessionCmds{"cinnamon","cinnamon-session"}
                        : SessionCmds{"cinnamon-session","cinnamon"};
    }
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
private:
    bool cinnamon_warning() const;
};

} // namespace tmoe::domain
