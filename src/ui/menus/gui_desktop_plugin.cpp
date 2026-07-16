/** GUI 桌面安装菜单插件 — 包装 GUIManager::run_desktop_install_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_desktop_menu(domain::GUIManager* gui) {
    auto menu = make_plugin_menu(
        _("menu.tui.gui_de"), _("menu.tui.title"), "plugin_gui_desktop");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.gui_de"), "enter_desktop",
        [gui](MenuContext&) -> bool {
            gui->run_desktop_install_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
