/** GUI 远程桌面菜单插件 — 将 GUIManager::run_remote_desktop_menu() 的 5 个子功能展平为独立菜单项。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_remote_desktop_menu(domain::GUIManager* gui) {
    auto menu = make_plugin_menu(
        _("menu.tui.gui_remote"), _("menu.tui.title"), "plugin_gui_remote");

    menu->add_child(LambdaAction::make(
        _("gui.remote_tightvnc"), "remote_tightvnc",
        [gui] { gui->run_vnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_x11vnc"), "remote_x11vnc",
        [gui] { gui->remote_desktop_manager().run_x11vnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_xsdl"), "remote_xsdl",
        [gui] { gui->run_xsdl_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_novnc"), "remote_novnc",
        [gui] { gui->remote_desktop_manager().run_novnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_xrdp"), "remote_xrdp",
        [gui] { gui->remote_desktop_manager().run_xrdp_menu(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
