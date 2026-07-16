/** DownloadTools 菜单插件 — 包装 DownloadTools::run_download_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/download_tools.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_download_menu(domain::DownloadTools* mgr) {
    auto menu = make_plugin_menu(
        _("download.menu_title"), _("download.menu_prompt"), "plugin_download");

    menu->add_child(std::make_shared<LambdaAction>(
        _("download.menu_title"), "enter_download",
        [mgr](MenuContext&) -> bool {
            mgr->run_download_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
