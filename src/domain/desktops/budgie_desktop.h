#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct BudgieDesktop : DesktopBase {
    explicit BudgieDesktop(const TmoeConfig& c) : DesktopBase(c, gui_config::all_desktops()[8]) {}
    void will_be_installed_message() const override;
    SessionCmds get_session_commands() const override;
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
    std::string session_ = "desktop"; // "panel" or "desktop"
private:
    bool budgie_warning() const;
};
}
