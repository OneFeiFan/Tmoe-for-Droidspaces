/** Virtualization 菜单插件 — 包装 VirtualizationManager::run_virt_menu()。
 *  内部 Docker 回调和 Wine 子菜单在旧 while 循环中保留工作。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/virtualization/virtualization.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_virtualization_menu(domain::VirtualizationManager* mgr) {
    auto menu = make_plugin_menu(
        _("virt.title"), _("virt.menu_prompt"), "plugin_virtualization");

    menu->add_child(std::make_shared<LambdaAction>(
        _("virt.title"), "enter_virt",
        [mgr](MenuContext&) -> bool {
            mgr->run_virt_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
