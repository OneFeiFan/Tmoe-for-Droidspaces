#pragma once
#include "desktop_base.h"

namespace tmoe::domain {

class XfceDesktop : public DesktopBase {
public:
    explicit XfceDesktop(const TmoeConfig& cfg);

    PreInstallChoices pre_install_choices(DistroFamily, bool) override;
    void post_install_config(const PostInstallContext&) override;
    void post_install_extras(const PostInstallContext&) override;
    void will_be_installed_message() const override;

protected:
    /// 允许子类传入不同 DesktopInfo（如 xfce-lite）
    XfceDesktop(const TmoeConfig& cfg, const DesktopInfo& info) : DesktopBase(cfg, info) {}
    /// qt5ct 环境变量配置 (xfce-lite 子类也需要)
    void setup_qt5ct_env();
    /// whiptail 确认对话框，返回 false 表示用户取消
    bool xfce_warning() const;

private:
    void install_debian_extras(const PostInstallContext& ctx);
    void download_breeze_cursor(const PostInstallContext& ctx);
    void configure_xfce_settings(const PostInstallContext& ctx);
    void xfce_color_scheme() const;
};

} // namespace tmoe::domain
