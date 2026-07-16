/** DevTools 菜单插件 — 每个 IDE/工具作为独立的 LambdaAction，包装原 domain 方法调用。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/dev_tools.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_dev_tools_menu(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        _("devtools.menu_title"), _("devtools.tightvnc_tip"), "plugin_devtools");

    // 1 — Visual Studio Code → vscode 子菜单 (Official/Server/Codium/fix)
    menu->add_child(std::make_shared<LambdaAction>(
        "Visual Studio Code", "dev_vscode",
        [mgr](MenuContext&) -> bool {
            mgr->run_vscode_menu();
            return true;
        }));

    // 2 — Android Studio → dev_menu_02 (install/delete pkg/remove)
    menu->add_child(std::make_shared<LambdaAction>(
        "Android Studio", "dev_android_studio",
        [mgr](MenuContext&) -> bool {
            mgr->prep_android_studio();
            mgr->run_ide_submenu_02();
            return true;
        }));

    // 3 — IntelliJ IDEA → 旗舰版/社区版选择 → dev_menu_01
    menu->add_child(std::make_shared<LambdaAction>(
        "IntelliJ IDEA", "dev_idea",
        [mgr](MenuContext&) -> bool {
            mgr->choose_idea_edition();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 4 — PyCharm Community → dev_menu_01
    menu->add_child(std::make_shared<LambdaAction>(
        "PyCharm", "dev_pycharm",
        [mgr](MenuContext&) -> bool {
            mgr->prep_pycharm();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 5 — WebStorm → dev_menu_01
    menu->add_child(std::make_shared<LambdaAction>(
        "WebStorm", "dev_webstorm",
        [mgr](MenuContext&) -> bool {
            mgr->prep_webstorm();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 6 — CLion → dev_menu_01
    menu->add_child(std::make_shared<LambdaAction>(
        "CLion", "dev_clion",
        [mgr](MenuContext&) -> bool {
            mgr->prep_clion();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 7 — GoLand → dev_menu_01
    menu->add_child(std::make_shared<LambdaAction>(
        "GoLand", "dev_goland",
        [mgr](MenuContext&) -> bool {
            mgr->prep_goland();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 8 — GNU Emacs → 包管理器快速安装
    menu->add_child(std::make_shared<LambdaAction>(
        "GNU Emacs", "dev_emacs",
        [mgr](MenuContext&) -> bool {
            mgr->install_emacs();
            return true;
        }));

    // 9 — Code::Blocks → 包管理器快速安装
    menu->add_child(std::make_shared<LambdaAction>(
        "Code::Blocks", "dev_codeblocks",
        [mgr](MenuContext&) -> bool {
            mgr->install_code_blocks();
            return true;
        }));

    // 10 — GitHub Desktop → dev_menu_01 (x64 only)
    menu->add_child(std::make_shared<LambdaAction>(
        "GitHub Desktop", "dev_github_desktop",
        [mgr](MenuContext&) -> bool {
            mgr->prep_github_desktop();
            mgr->run_ide_submenu_01();
            return true;
        }));

    // 11 — Sublime Text → 添加官方源 + 安装
    menu->add_child(std::make_shared<LambdaAction>(
        "Sublime Text", "dev_sublime",
        [mgr](MenuContext&) -> bool {
            mgr->install_sublime_text();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
