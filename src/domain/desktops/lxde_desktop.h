#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class LxdeDesktop : public DesktopBase {
public:
    explicit LxdeDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    PreInstallChoices pre_install_choices(
        DistroFamily family, bool is_auto_mode) override;
    void post_install_config(const PostInstallContext& ctx) override;
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
};

} // namespace tmoe::domain
