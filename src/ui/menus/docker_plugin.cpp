/** Docker 菜单插件 — 将 run_docker_menu() 的每个菜单项展开为独立的 LambdaAction。 */
#include "ui/menus/docker_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/virtualization/docker.h"

namespace tmoe::ui::menus {

    std::shared_ptr<IUIMenu> DockerMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("docker.title"), _("docker.menu_prompt"), "plugin_docker");

        menu->add_action(_("docker.menu_pull_distro"), "docker_pull_distro",
                         [this] { mgr_->choose_gnu_linux_docker_images(); });

        menu->add_action(_("docker.menu_portainer"), "docker_portainer",
                         [this] { mgr_->install_docker_portainer(); });

        menu->add_action(_("docker.menu_export"), "docker_export",
                         [this] { mgr_->export_docker_image_menu(); });

        menu->add_action(_("docker.menu_import"), "docker_import",
                         [this] { mgr_->import_docker_image_menu(); });

        menu->add_action(_("docker.menu_mirror"), "docker_mirror",
                         [this] { mgr_->docker_mirror_source(); });

        menu->add_action(_("docker.menu_install_ce"), "docker_install_ce",
                         [this] { mgr_->install_docker_ce_or_io(); });

        menu->add_action(_("docker.menu_add_user"), "docker_add_user",
                         [this] { mgr_->add_user_to_docker_group(); });

        add_sandwich_nav(menu);
        return menu;
    }

} // namespace tmoe::ui::menus
