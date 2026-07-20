/** Virtualization 菜单插件 — 使用 IUIMenu 框架直接构建菜单树。
 *  Docker 项通过 VirtualizationManager::invoke_docker() 触发回调，
 *  Wine 项展开为嵌套子菜单（LambdaAction + MenuEngine 启动 wine 子菜单）。 */
#include "ui/menus/virtualization_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/virtualization/virtualization.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> VirtualizationMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("virt.title"), _("virt.menu_prompt"), "plugin_virtualization");

    // Docker — 触发 Manager 中注入的回调（→ DockerManager::run_docker_menu）
    menu->add_child(LambdaAction::make(
        _("virt.docker"), "virt_docker",
        [this]() { mgr_->invoke_docker(); }));

    // ── Wine 子菜单 ──
    auto wine_menu = make_plugin_menu(
        _("virt.wine_title"), _("virt.wine_prompt"), "plugin_virt_wine");

    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_devel"), "wine_devel",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_wine("devel");
            Logger::press_enter();
            return ok;
        }));
    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_staging"), "wine_staging",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_wine("staging");
            Logger::press_enter();
            return ok;
        }));
    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_stable"), "wine_stable",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_wine("stable");
            Logger::press_enter();
            return ok;
        }));
    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_winetricks"), "wine_winetricks",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_winetricks();
            Logger::press_enter();
            return ok;
        }));
    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_dxvk"), "wine_dxvk",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_dxvk();
            Logger::press_enter();
            return ok;
        }));
    wine_menu->add_child(std::make_shared<LambdaAction>(
        _("virt.wine_playonlinux"), "wine_playonlinux",
        [this](MenuContext &) -> bool {
            bool ok = mgr_->install_playonlinux();
            Logger::press_enter();
            return ok;
        }));

    // Wine 是嵌套子菜单 → add_navigation_items
    add_navigation_items(wine_menu);

    // Wine 入口 — add_submenu
    menu->add_submenu(_("virt.wine"), "virt_wine", wine_menu);

    // 顶层菜单 → add_sandwich_nav
    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
