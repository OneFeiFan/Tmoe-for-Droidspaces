#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class KdeDesktop : public DesktopBase {
public:
    explicit KdeDesktop(const TmoeConfig& cfg);

    PreInstallChoices pre_install_choices(
        DistroFamily family, bool is_auto_mode) override;
    void post_install_config(const PostInstallContext& ctx) override;
    void post_install_extras(const PostInstallContext& ctx) override;
    void will_be_installed_message() const override;

private:
    bool kde_warning() const;
    void choose_wayland_or_x11(const PostInstallContext& ctx);
    void plasma_wayland_env();
};

} // namespace tmoe::domain
