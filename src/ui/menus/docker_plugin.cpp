/** Docker 菜单插件 — 包装 DockerManager::run_docker_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/virtualization/docker.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_docker_menu(domain::DockerManager* mgr) {
    auto menu = make_plugin_menu(
        _("docker.title"), _("docker.menu_prompt"), "plugin_docker");

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.title"), "enter_docker",
        [mgr](MenuContext&) -> bool {
            mgr->run_docker_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
