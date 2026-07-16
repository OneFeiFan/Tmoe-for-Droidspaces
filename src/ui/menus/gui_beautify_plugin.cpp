/** GUI 美化菜单插件 — BeautifyMenuPlugin 类。
 *  6 个独立操作：主题 / 图标 / 壁纸 / 鼠标光标 / 停靠栏 / Compiz。 */
#include "ui/menus/gui_beautify_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

BeautifyMenuPlugin::BeautifyMenuPlugin(domain::GUIManager* gui) : gui_(gui) {}

std::shared_ptr<IUIMenu> BeautifyMenuPlugin::build() {
    auto& bm = gui_->beautification_manager_;

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
