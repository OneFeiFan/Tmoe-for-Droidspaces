/** DevTools 菜单插件 — 包装 DeveloperTools::run_dev_tools_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/dev_tools.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_dev_tools_menu(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        _("devtools.menu_title"), _("devtools.menu_title"), "plugin_devtools");

    menu->add_child(std::make_shared<LambdaAction>(
        _("devtools.menu_title"), "enter_devtools",
        [mgr](MenuContext&) -> bool {
            mgr->run_dev_tools_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
