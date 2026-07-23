#pragma once

#include "desktop_base.h"

namespace tmoe::domain {
    struct UkuiDesktop : DesktopBase {
        explicit UkuiDesktop(const TmoeConfig &c) : DesktopBase(c, gui_config::all_desktops()[11]) {}

        void will_be_installed_message() const override;

        SessionCmds get_session_commands() const override {
            bool is_proot = (cfg_.is_termux || cfg_.linux_distro == "Android");
            return is_proot ? SessionCmds{"ukui-panel", "ukui-session"} : SessionCmds{"ukui-session", "ukui-panel"};
        }

        PreInstallChoices pre_install_choices(DistroFamily, bool) override;

        void post_install_config(const PostInstallContext &) override;

        void post_install_extras(const PostInstallContext &) override;
    };
}
