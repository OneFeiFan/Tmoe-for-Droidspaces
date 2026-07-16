/** Browser 菜单插件 — 包装 BrowserManager::run_browser_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/browser.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_browser_menu(domain::BrowserManager* mgr) {
    auto menu = make_plugin_menu(
        _("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser");

    menu->add_child(std::make_shared<LambdaAction>(
        _("browser.menu_title"), "enter_browser",
        [mgr](MenuContext&) -> bool {
            mgr->run_browser_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
