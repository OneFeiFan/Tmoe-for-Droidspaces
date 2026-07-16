/** Education 菜单插件 — 将 run_education_menu() 的 6 个 L2 子菜单
 *  展开为独立的 LambdaAction，由框架 whiptail 引擎统一驱动。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/features/education.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_education_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.menu_title"), _("edu.menu_prompt"), "plugin_education");

    // 每个 L1 菜单项 → 独立的 LambdaAction，调用对应的 L2 子菜单方法
    menu->add_child(LambdaAction::make(
        _("edu.gaokao"), "edu_gaokao",
        [mgr] { mgr->run_gaokao_menu(); }));

    menu->add_child(LambdaAction::make(
        _("edu.kaoyan"), "edu_kaoyan",
        [mgr] { mgr->run_kaoyan_menu(); }));

    menu->add_child(LambdaAction::make(
        _("edu.math"), "edu_math",
        [mgr] { mgr->run_math_menu(); }));

    menu->add_child(LambdaAction::make(
        _("edu.english"), "edu_english",
        [mgr] { mgr->run_english_menu(); }));

    menu->add_child(LambdaAction::make(
        _("edu.physics"), "edu_physics",
        [mgr] { mgr->run_physics_menu(); }));

    menu->add_child(LambdaAction::make(
        _("edu.chemistry"), "edu_chemistry",
        [mgr] { mgr->run_chemistry_menu(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
