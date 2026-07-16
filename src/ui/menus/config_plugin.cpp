/** Config 菜单插件 — 包装 ConfigManager::run_config_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/config_manager.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_config_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.config"), _("config.menu_prompt"), "plugin_config");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.config"), "enter_config",
        [mgr](MenuContext&) -> bool {
            mgr->run_config_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
