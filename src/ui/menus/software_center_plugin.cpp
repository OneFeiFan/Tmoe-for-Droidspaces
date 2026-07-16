/** SoftwareCenter 菜单插件 — 将 run_software_center_menu() 的 13 个菜单项
 *  分解为独立的 LambdaAction，通过 IUIMenu 框架渲染和分发。
 *  跨模块回调（Browser/Dev/Office/Download/Zsh）通过 SoftwareCenter 的
 *  公开 invoker 方法调用；子菜单直接调用 SoftwareCenter 的公共方法。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/software_center.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_software_center_menu(domain::SoftwareCenter* mgr) {
    auto menu = make_plugin_menu(
        _("swcenter.menu_title"), _("swcenter.menu_prompt"), "plugin_software_center");

    // 1: Browser → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.browser"), "1",
        [mgr](MenuContext&) -> bool {
            mgr->invoke_browser_cb();
            return true;
        }));

    // 2: Dev → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.dev"), "2",
        [mgr](MenuContext&) -> bool {
            mgr->invoke_dev_cb();
            return true;
        }));

    // 3: Electron → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron"), "3",
        [mgr](MenuContext&) -> bool {
            mgr->run_electron_apps_menu();
            return true;
        }));

    // 4: Media → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.media"), "4",
        [mgr](MenuContext&) -> bool {
            mgr->run_media_menu();
            return true;
        }));

    // 5: SNS → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns"), "5",
        [mgr](MenuContext&) -> bool {
            mgr->run_sns_menu();
            return true;
        }));

    // 6: Games → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games"), "6",
        [mgr](MenuContext&) -> bool {
            mgr->run_games_menu();
            return true;
        }));

    // 7: Docs → cross-module callback (office)
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.docs"), "7",
        [mgr](MenuContext&) -> bool {
            mgr->invoke_office_cb();
            return true;
        }));

    // 8: Debian Opt Repo → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.debian_opt"), "8",
        [mgr](MenuContext&) -> bool {
            mgr->run_debian_opt_menu();
            return true;
        }));

    // 9: Download → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.download"), "9",
        [mgr](MenuContext&) -> bool {
            mgr->invoke_download_cb();
            return true;
        }));

    // 10: Pkg GUI → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.pkg_gui"), "10",
        [mgr](MenuContext&) -> bool {
            mgr->run_pkg_gui_menu();
            return true;
        }));

    // 11: Zsh → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.zsh"), "11",
        [mgr](MenuContext&) -> bool {
            mgr->invoke_zsh_cb();
            return true;
        }));

    // 12: File Shared → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.fileshare"), "12",
        [mgr](MenuContext&) -> bool {
            mgr->run_fileshare_menu();
            return true;
        }));

    // 13: Cleanup → sub-menu
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.cleanup"), "13",
        [mgr](MenuContext&) -> bool {
            mgr->run_cleanup_menu();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
