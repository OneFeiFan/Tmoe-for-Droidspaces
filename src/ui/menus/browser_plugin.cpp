/** Browser 菜单插件 — 将 run_browser_menu() 的 6 个子项展平为独立 LambdaAction。 */
#include "ui/menus/browser_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/browser.h"

namespace tmoe::ui::menus {

BrowserMenuPlugin::BrowserMenuPlugin(domain::BrowserManager* mgr) : mgr_(mgr) {}

std::shared_ptr<IUIMenu> BrowserMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser");

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_firefox_chromium"), "browser_firefox_chromium",
        [this] {
            mgr_->firefox_or_chromium();
            Logger::press_enter();
        }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_edge"), "browser_edge",
        [this] {
            mgr_->microsoft_edge_menu();
            Logger::press_enter();
        }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_falkon"), "browser_falkon",
        [this] {
            mgr_->falkon_browser_menu();
            Logger::press_enter();
        }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_vivaldi"), "browser_vivaldi",
        [this] {
            mgr_->install_vivaldi();
            Logger::press_enter();
        }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_epiphany"), "browser_epiphany",
        [this] {
            mgr_->install_epiphany();
            Logger::press_enter();
        }));

    menu->add_child(LambdaAction::make(
        _("browser.menu_item_midori"), "browser_midori",
        [this] {
            mgr_->install_midori();
            Logger::press_enter();
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
