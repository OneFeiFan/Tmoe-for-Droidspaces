/** BetaFeatures 菜单插件 — 包装 BetaFeaturesManager::run_beta_menu()。
 *  内部跨模块回调（Virt/Education/InputMethod/TerminalApp）在旧 while 循环中保留工作。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/features/beta_features.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_beta_features_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.menu_title"), _("beta.menu_prompt"), "plugin_beta_features");

    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.menu_title"), "enter_beta",
        [mgr](MenuContext&) -> bool {
            mgr->run_beta_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
