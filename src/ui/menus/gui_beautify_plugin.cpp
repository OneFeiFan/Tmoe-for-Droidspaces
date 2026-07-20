/** GUI 美化菜单插件 — BeautifyMenuPlugin 类。
 *  6 个独立操作：主题 / 图标 / 壁纸 / 鼠标光标 / 停靠栏 / Compiz。 */
#include "ui/menus/gui_beautify_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> BeautifyMenuPlugin::build() {
    auto& bm = mgr_->beautification_manager_;

    auto menu = make_plugin_menu(
        _("gui.beautify_title"), _("gui.beautify_prompt"), "gui_beautify");

    menu->add_child(LambdaAction::make(
        _("gui.beautify.themes"), "1",
        [&bm] { bm.configure_theme_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.icon_theme"), "2",
        [&bm] { bm.download_icon_themes_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.wallpaper"), "3",
        [&bm] { bm.download_wallpapers_menu(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.mouse_cursor"), "4",
        [&bm] { bm.configure_mouse_cursor(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.dock"), "5",
        [&bm] { bm.install_dock(); }));

    menu->add_child(LambdaAction::make(
        _("gui.beautify.compiz"), "6",
        [&bm] { bm.install_compiz(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
