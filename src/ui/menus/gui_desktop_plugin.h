#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class GUIManager; }

namespace tmoe::ui::menus {

/** 桌面环境安装菜单插件。
 *  构建 4 个子菜单：免 Root 桌面 / 需 Root 桌面 / 窗口管理器 / 显示管理器。 */
class DesktopMenuPlugin : public PluginFor<domain::GUIManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:

    std::shared_ptr<IUIMenu> build_rootless_menu();
    std::shared_ptr<IUIMenu> build_rootful_menu();
    std::shared_ptr<IUIMenu> build_wm_menu();
    std::shared_ptr<IUIMenu> build_dm_menu();
};

} // namespace tmoe::ui::menus
