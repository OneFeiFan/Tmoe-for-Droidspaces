#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class VirtualizationManager; }

namespace tmoe::ui::menus {

/** 虚拟化菜单插件。
 *  Docker 项通过 VirtualizationManager::invoke_docker() 触发回调，
 *  Wine 项展开为嵌套子菜单（6 个子项 + 导航）。 */
class VirtualizationMenuPlugin : public IPlugin {
public:
    explicit VirtualizationMenuPlugin(domain::VirtualizationManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::VirtualizationManager* mgr_;
};

} // namespace tmoe::ui::menus
