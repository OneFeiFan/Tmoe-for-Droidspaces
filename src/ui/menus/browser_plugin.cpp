/** Browser 菜单插件 — 将 run_browser_menu() 的 6 个子项展平为独立 LambdaAction。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/browser.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_browser_menu(domain::BrowserManager* mgr) {
    auto menu = make_plugin_menu(
        _("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser");

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_firefox_chromium"), "browser_firefox_chromium",
        [mgr] { mgr->firefox_or_chromium(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_edge"), "browser_edge",
        [mgr] { mgr->microsoft_edge_menu(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_falkon"), "browser_falkon",
        [mgr] { mgr->falkon_browser_menu(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_vivaldi"), "browser_vivaldi",
        [mgr] { mgr->install_vivaldi(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_epiphany"), "browser_epiphany",
        [mgr] { mgr->install_epiphany(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_midori"), "browser_midori",
        [mgr] { mgr->install_midori(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
