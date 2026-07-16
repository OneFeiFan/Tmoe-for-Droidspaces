/** Education 菜单插件 — 包装 EducationManager::run_education_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/features/education.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_education_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.menu_title"), _("edu.menu_prompt"), "plugin_education");

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.menu_title"), "enter_education",
        [mgr](MenuContext&) -> bool {
            mgr->run_education_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
