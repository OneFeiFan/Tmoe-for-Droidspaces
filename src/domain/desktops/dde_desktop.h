#pragma once
#include "desktop_base.h"
namespace tmoe::domain {
struct DdeDesktop : DesktopBase {
    explicit DdeDesktop(const TmoeConfig& c) : DesktopBase(c, gui_config::all_desktops()[9]) {}
    void will_be_installed_message() const override;
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
private:
    bool dde_warning() const;
};
}
