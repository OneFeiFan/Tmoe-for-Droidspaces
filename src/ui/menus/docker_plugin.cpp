/** Docker 菜单插件 — 将 run_docker_menu() 的每个菜单项展开为独立的 LambdaAction。 */
#include "ui/menus/docker_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/virtualization/docker.h"

namespace tmoe::ui::menus {

DockerMenuPlugin::DockerMenuPlugin(domain::DockerManager* mgr) : mgr_(mgr) {}

std::shared_ptr<IUIMenu> DockerMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("docker.title"), _("docker.menu_prompt"), "plugin_docker");

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_pull_distro"), "docker_pull_distro",
        [this](MenuContext&) -> bool {
            mgr_->choose_gnu_linux_docker_images();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_portainer"), "docker_portainer",
        [this](MenuContext&) -> bool {
            mgr_->install_docker_portainer();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_export"), "docker_export",
        [this](MenuContext&) -> bool {
            mgr_->export_docker_image_menu();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_import"), "docker_import",
        [this](MenuContext&) -> bool {
            mgr_->import_docker_image_menu();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_mirror"), "docker_mirror",
        [this](MenuContext&) -> bool {
            mgr_->docker_mirror_source();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_install_ce"), "docker_install_ce",
        [this](MenuContext&) -> bool {
            mgr_->install_docker_ce_or_io();
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("docker.menu_add_user"), "docker_add_user",
        [this](MenuContext&) -> bool {
            mgr_->add_user_to_docker_group();
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
