#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class XfceDesktop : public DesktopBase {
public:
    explicit XfceDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    bool recommends_tiger_vnc() const override { return true; }
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
    void install_debian_extras(const PostInstallContext& ctx);
    void setup_qt5ct_env();
    void download_breeze_cursor(const PostInstallContext& ctx);
    void configure_xfce_settings(const PostInstallContext& ctx);
    void xfce_warning() const;
    void xfce_color_scheme() const;
};

} // namespace tmoe::domain
