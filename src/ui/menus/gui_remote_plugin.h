#pragma once

#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class GUIManager; }

namespace tmoe::ui::menus {

/** 远程桌面配置菜单插件。
 *  5 个远程桌面类型：TightVNC / x11vnc / XSDL / noVNC / XRDP。 */
    class RemoteDesktopMenuPlugin : public PluginFor<domain::GUIManager> {
    public:
        using PluginFor::PluginFor;

        std::shared_ptr<IUIMenu> build() override;
    };

} // namespace tmoe::ui::menus
