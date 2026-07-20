#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class MateDesktop : public DesktopBase {
public:
    explicit MateDesktop(const TmoeConfig& cfg);

    PreInstallChoices pre_install_choices(
        DistroFamily family, bool is_auto_mode) override;
    void post_install_config(const PostInstallContext& ctx) override;
    void post_install_extras(const PostInstallContext& ctx) override;
    void will_be_installed_message() const override;
};

} // namespace tmoe::domain
