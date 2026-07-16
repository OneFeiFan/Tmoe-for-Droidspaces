/** GUI 远程桌面菜单插件 — RemoteDesktopMenuPlugin 类。
 *  5 个远程桌面类型：TightVNC / x11vnc / XSDL / noVNC / XRDP。 */
#include "ui/menus/gui_remote_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

RemoteDesktopMenuPlugin::RemoteDesktopMenuPlugin(domain::GUIManager* gui) : gui_(gui) {}

std::shared_ptr<IUIMenu> RemoteDesktopMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("gui.remote_title"), _("gui.remote_prompt"), "gui_remote");

    menu->add_child(LambdaAction::make(
        _("gui.remote_tightvnc"), "1",
        [this] { gui_->run_vnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_x11vnc"), "2",
        [this] { gui_->remote_desktop_manager().run_x11vnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_xsdl"), "3",
        [this] { gui_->run_xsdl_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_novnc"), "4",
        [this] { gui_->remote_desktop_manager().run_novnc_config_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.remote_xrdp"), "5",
        [this] { gui_->remote_desktop_manager().run_xrdp_menu(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
