#include "main_menu.h"
#include "ui/builtin_actions.h"
#include "ui/registry.h"
#include "core/i18n.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> MenuBuilder::build_from_registry(
    const std::string& title)
{
    auto menu = std::make_shared<SimpleMenu>(
        title, _("menu.tui.title"), "plugin_main");

    // 从全局注册表收集所有已注册插件
    auto plugins = MenuRegistry::items();
    for (auto& item : plugins) {
        if (item) {
            menu->add_child(item);
        }
    }

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
