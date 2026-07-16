/** InputMethod 菜单插件 — 将 run_input_method_menu() 的 5 个子项暴露为 IUIMenu 条目。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/input_method.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_input_method_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.menu_title"), _("input.menu_prompt"), "plugin_input_method");

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4"), "input_fcitx4",
        [mgr](MenuContext&) -> bool {
            mgr->run_fcitx4_menu();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5"), "input_fcitx5",
        [mgr](MenuContext&) -> bool {
            mgr->run_fcitx5_menu();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus"), "input_ibus",
        [mgr](MenuContext&) -> bool {
            mgr->run_ibus_menu();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.sogou"), "input_sogou",
        [mgr](MenuContext&) -> bool {
            mgr->install_sogou();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.faq"), "input_faq",
        [mgr](MenuContext&) -> bool {
            mgr->show_input_faq();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
