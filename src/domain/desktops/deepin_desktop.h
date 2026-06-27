#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct DeepinDesktop : DesktopBase {
    explicit DeepinDesktop(const TmoeConfig& c) : DesktopBase(c), info_(gui_config::all_desktops()[10]) {}
    std::string get_id() const override { return info_.id; }
    const DesktopInfo& get_info() const override { return info_; }
    bool recommends_tiger_vnc() const override { return true; }
    void will_be_installed_message() const override { Logger::info("Deepin: deepin-session / deepin-launcher"); }
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
private: const DesktopInfo& info_;
};
}
