#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct BudgieDesktop : DesktopBase {
    explicit BudgieDesktop(const TmoeConfig& c) : DesktopBase(c), info_(gui_config::all_desktops()[8]) {}
    std::string get_id() const override { return info_.id; }
    const DesktopInfo& get_info() const override { return info_; }
    bool recommends_tiger_vnc() const override { return true; }
    void will_be_installed_message() const override { Logger::info("Budgie: budgie-desktop / budgie-panel"); }
    SessionCmds get_session_commands() const override;
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
    std::string session_ = "desktop"; // "panel" or "desktop"
private: const DesktopInfo& info_;
};
}
