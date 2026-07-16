/** TerminalApp 菜单插件 — 包装 TerminalAppManager::run_terminal_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/terminal_app.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_terminal_app_menu(domain::TerminalAppManager* mgr) {
    auto menu = make_plugin_menu(
        _("term.menu_title"), _("term.menu_prompt"), "plugin_terminal_app");

    menu->add_child(std::make_shared<LambdaAction>(
        _("term.menu_title"), "enter_terminal_app",
        [mgr](MenuContext&) -> bool {
            mgr->run_terminal_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
