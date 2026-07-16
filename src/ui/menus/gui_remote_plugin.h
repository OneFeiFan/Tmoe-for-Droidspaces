#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class GUIManager; }

namespace tmoe::ui::menus {

/** 远程桌面配置菜单插件。
 *  5 个远程桌面类型：TightVNC / x11vnc / XSDL / noVNC / XRDP。 */
class RemoteDesktopMenuPlugin : public IPlugin {
public:
    explicit RemoteDesktopMenuPlugin(domain::GUIManager* gui);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::GUIManager* gui_;
};

} // namespace tmoe::ui::menus
