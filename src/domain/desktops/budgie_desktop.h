#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct BudgieDesktop : DesktopBase {
    explicit BudgieDesktop(const TmoeConfig& c) : DesktopBase(c), info_(gui_config::all_desktops()[8]) {}
    std::string get_id() const override { return info_.id; }
    const DesktopInfo& get_info() const override { return info_; }
    bool recommends_tiger_vnc() const override { return true; }
    void will_be_installed_message() const override { Logger::info("Budgie: budgie-desktop / budgie-panel"); }
    SessionCmds get_session_commands() const override { return {"budgie-desktop-builtin","budgie-panel"}; }
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
private: const DesktopInfo& info_;
};
}
