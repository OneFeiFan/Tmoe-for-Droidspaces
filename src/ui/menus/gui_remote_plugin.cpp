/** GUI 远程桌面菜单插件 — 包装 GUIManager::run_remote_desktop_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_remote_desktop_menu(domain::GUIManager* gui) {
    auto menu = make_plugin_menu(
        _("menu.tui.gui_remote"), _("menu.tui.title"), "plugin_gui_remote");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.gui_remote"), "enter_remote_desktop",
        [gui](MenuContext&) -> bool {
            gui->run_remote_desktop_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
