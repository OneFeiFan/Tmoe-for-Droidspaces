#pragma once
#include "xfce_desktop.h"

namespace tmoe::domain {

/** Xfce 精简版。与完整版共享大部分 post_install_config，
 *  但跳过壁纸、papirus 图标主题等外观美化步骤。 */
class XfceLiteDesktop : public XfceDesktop {
public:
    explicit XfceLiteDesktop(const TmoeConfig& cfg);

    std::string get_id() const override;
    const DesktopInfo& get_info() const override;

    void post_install_extras(const PostInstallContext& ctx) override;
    void will_be_installed_message() const override;

private:
    const DesktopInfo& info_;
};

} // namespace tmoe::domain
