#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class DeveloperTools; }

namespace tmoe::ui::menus {

/** 开发工具菜单插件。
 *  嵌套子菜单：VS Code / IDE type1 (JetBrains/GitHub) / IDE type2 (Android Studio)。 */
class DevToolsMenuPlugin : public IPlugin {
public:
    explicit DevToolsMenuPlugin(domain::DeveloperTools* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::DeveloperTools* mgr_;

    std::shared_ptr<IUIMenu> build_vscode_menu();
    std::shared_ptr<IUIMenu> build_ide_submenu_01();
    std::shared_ptr<IUIMenu> build_ide_submenu_02();
};

} // namespace tmoe::ui::menus
