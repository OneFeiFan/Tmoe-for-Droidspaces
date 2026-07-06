#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct CutefishDesktop : DesktopBase {
    explicit CutefishDesktop(const TmoeConfig& c) : DesktopBase(c), info_(gui_config::all_desktops()[12]) {}
    std::string get_id() const override { return info_.id; }
    const DesktopInfo& get_info() const override { return info_; }
    void will_be_installed_message() const override { Logger::info("Cutefish: cutefish-session / cutefish-launcher"); }
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
private: const DesktopInfo& info_;
};
}
