/** GUI 美化菜单插件 — 将 BeautificationManager 的 6 个子功能展平为独立菜单项。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_beautify_menu(domain::GUIManager* gui) {
    auto& bm = gui->beautification_manager_;

    auto menu = make_plugin_menu(
        _("menu.tui.gui_beautify"), _("menu.tui.title"), "plugin_gui_beautify");

    menu->add_child(LambdaAction::make(
        _("gui.beautify.themes"), "beautify_themes",
        [&bm] { bm.configure_theme_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.icon_theme"), "beautify_icon_theme",
        [&bm] { bm.download_icon_themes_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.wallpaper"), "beautify_wallpaper",
        [&bm] { bm.download_wallpapers_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.mouse_cursor"), "beautify_mouse_cursor",
        [&bm] { bm.configure_mouse_cursor(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.dock"), "beautify_dock",
        [&bm] { bm.install_dock(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.compiz"), "beautify_compiz",
        [&bm] { bm.install_compiz(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
