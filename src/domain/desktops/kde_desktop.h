#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class KdeDesktop : public DesktopBase {
public:
    explicit KdeDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    PreInstallChoices pre_install_choices(
        DistroFamily family, bool is_auto_mode) override;
    void post_install_config(const PostInstallContext& ctx) override;
    bool recommends_tiger_vnc() const override { return true; }
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
    void kde_warning() const;
    void choose_wayland_or_x11(const PostInstallContext& ctx);
    void plasma_wayland_env();
};

} // namespace tmoe::domain
