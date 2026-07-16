/** InputMethod 菜单插件 — 包装 InputMethodManager::run_input_method_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/input_method.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_input_method_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.menu_title"), _("input.menu_prompt"), "plugin_input_method");

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.menu_title"), "enter_input_method",
        [mgr](MenuContext&) -> bool {
            mgr->run_input_method_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
