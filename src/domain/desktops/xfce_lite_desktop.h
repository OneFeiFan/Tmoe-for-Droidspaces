#pragma once
#include "xfce_desktop.h"

namespace tmoe::domain {

/** Xfce 精简版。覆盖 post_install_config 跳过 debian_extras/配色/主题配置，
 *  覆盖 post_install_extras 跳过壁纸/papirus 图标。 */
class XfceLiteDesktop : public XfceDesktop {
public:
    explicit XfceLiteDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    void post_install_config(const PostInstallContext& ctx) override;
    void post_install_extras(const PostInstallContext& ctx) override;
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
};

} // namespace tmoe::domain
