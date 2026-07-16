/** Office 菜单插件 — 包装 OfficeManager::run_office_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/office.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_office_menu(domain::OfficeManager* mgr) {
    auto menu = make_plugin_menu(
        _("office.menu_title"), _("office.menu_prompt"), "plugin_office");

    menu->add_child(std::make_shared<LambdaAction>(
        _("office.menu_title"), "enter_office",
        [mgr](MenuContext&) -> bool {
            mgr->run_office_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
