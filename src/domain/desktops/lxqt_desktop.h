#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class LxqtDesktop : public DesktopBase {
public:
    explicit LxqtDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    SessionCmds get_session_commands() const override {
        auto f = infer_family_from_config(cfg_.linux_distro);
        if (f == DistroFamily::Alpine) return {"openbox", ""};
        return DesktopBase::get_session_commands();
    }
    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
};

} // namespace tmoe::domain
