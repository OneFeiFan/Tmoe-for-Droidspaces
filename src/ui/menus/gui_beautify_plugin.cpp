/** GUI 美化菜单插件 — 包装 GUIManager::beautification_manager_.run_beautification_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_beautify_menu(domain::GUIManager* gui) {
    auto menu = make_plugin_menu(
        _("menu.tui.gui_beautify"), _("menu.tui.title"), "plugin_gui_beautify");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.gui_beautify"), "enter_beautify",
        [gui](MenuContext&) -> bool {
            gui->beautification_manager_.run_beautification_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
