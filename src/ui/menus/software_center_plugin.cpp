/** SoftwareCenter 菜单插件 — 包装 SoftwareCenter::run_software_center_menu()。
 *  内部跨模块回调（Browser/DevTools/Office/DownloadTools/Zsh）在旧 while 循环中保留工作。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/software_center.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_software_center_menu(domain::SoftwareCenter* mgr) {
    auto menu = make_plugin_menu(
        _("swcenter.menu_title"), _("swcenter.menu_prompt"), "plugin_software_center");

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.menu_title"), "enter_swcenter",
        [mgr](MenuContext&) -> bool {
            mgr->run_software_center_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
