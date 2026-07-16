/** Docker 菜单插件 — 将 run_docker_menu() 的每个菜单项展开为独立的 LambdaAction。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/virtualization/docker.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_docker_menu(domain::DockerManager* mgr) {
    auto menu = make_plugin_menu(
        _("docker.title"), _("docker.menu_prompt"), "plugin_docker");

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_pull_distro"), "docker_pull_distro",
        [mgr](MenuContext&) -> bool {
            mgr->choose_gnu_linux_docker_images();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_portainer"), "docker_portainer",
        [mgr](MenuContext&) -> bool {
            mgr->install_docker_portainer();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_export"), "docker_export",
        [mgr](MenuContext&) -> bool {
            mgr->export_docker_image_menu();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_import"), "docker_import",
        [mgr](MenuContext&) -> bool {
            mgr->import_docker_image_menu();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_mirror"), "docker_mirror",
        [mgr](MenuContext&) -> bool {
            mgr->docker_mirror_source();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_install_ce"), "docker_install_ce",
        [mgr](MenuContext&) -> bool {
            mgr->install_docker_ce_or_io();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_add_user"), "docker_add_user",
        [mgr](MenuContext&) -> bool {
            mgr->add_user_to_docker_group();
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
