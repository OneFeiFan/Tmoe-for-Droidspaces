/** Termux 菜单插件 — 包装 TermuxManager::run_termux_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/termux.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_termux_menu(domain::TermuxManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.termux"), _("termux.menu_prompt"), "plugin_termux");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.termux"), "enter_termux",
        [mgr](MenuContext&) -> bool {
            mgr->run_termux_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
