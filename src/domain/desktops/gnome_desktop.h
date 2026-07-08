#pragma once
#include "desktop_base.h"

namespace tmoe::domain {
    struct GnomeDesktop : DesktopBase {
        explicit GnomeDesktop(const TmoeConfig &c) : DesktopBase(c), info_(gui_config::all_desktops()[7]) {
        }

        std::string get_id() const override { return info_.id; }
        const DesktopInfo &get_info() const override { return info_; }
        bool recommends_tiger_vnc() const override { return true; }
        void will_be_installed_message() const override;

        SessionCmds get_session_commands() const override;

        PreInstallChoices pre_install_choices(DistroFamily, bool) override;

        void post_install_config(const PostInstallContext &) override;
        void post_install_extras(const PostInstallContext &) override;

        const DesktopInfo &info_;
        std::string session_ = "1";

        void write_session_scripts();
    private:
        bool gnome_warning() const;
    };
} // namespace tmoe::domain
