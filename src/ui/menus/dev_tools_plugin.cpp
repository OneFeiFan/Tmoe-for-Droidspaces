/** DevTools 菜单插件 — 每个 IDE/工具作为独立的 LambdaAction 或嵌套子菜单。
 *  VS Code / IDE (type1/type2) 不再调用旧的 whiptail 子菜单，
 *  而是构建 SimpleMenu + LambdaAction 子菜单树。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/apps/dev_tools.h"

namespace tmoe::ui::menus {

// ── VS Code 子菜单 ─────────────────────────────────────

static std::shared_ptr<IUIMenu> build_vscode_menu(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        "Visual Studio Code", _("devtools.vscode_tip"), "plugin_dev_vscode");

    menu->add_child(std::make_shared<LambdaAction>(
        "Microsoft Official", "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_vscode_official();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "VS Code Server", "2",
        [mgr](MenuContext&) -> bool {
            mgr->run_vscode_server_menu();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "VS Codium", "3",
        [mgr](MenuContext&) -> bool {
            mgr->install_vscodium();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "Fix tightvnc vscode", "4",
        [mgr](MenuContext&) -> bool {
            mgr->fix_tightvnc_vscode();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── IDE 子菜单 type 1 (JetBrains / GitHub Desktop) ──────

static std::shared_ptr<IUIMenu> build_ide_submenu_01(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        "IDE Manager", "Install / Delete / Remove", "plugin_dev_ide_01");

    menu->add_child(std::make_shared<LambdaAction>(
        "Install/Upgrade", "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_ide_01();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "Delete Package", "2",
        [mgr](MenuContext&) -> bool {
            mgr->delete_ide_pkg();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "Remove", "3",
        [mgr](MenuContext&) -> bool {
            mgr->remove_ide_01();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── IDE 子菜单 type 2 (Android Studio) ─────────────────

static std::shared_ptr<IUIMenu> build_ide_submenu_02(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        "IDE Manager", "Install / Delete / Remove", "plugin_dev_ide_02");

    menu->add_child(std::make_shared<LambdaAction>(
        "Install/Upgrade", "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_ide_02();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "Delete Package", "2",
        [mgr](MenuContext&) -> bool {
            mgr->delete_ide_pkg_02();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        "Remove", "3",
        [mgr](MenuContext&) -> bool {
            mgr->remove_ide_02();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── 顶层入口 ──────────────────────────────────────────────

std::shared_ptr<IUIMenu> create_dev_tools_menu(domain::DeveloperTools* mgr) {
    auto menu = make_plugin_menu(
        _("devtools.menu_title"), _("devtools.tightvnc_tip"), "plugin_devtools");

    // 1 — Visual Studio Code → vscode 子菜单 (Official/Server/Codium/fix)
    auto vscode_menu = build_vscode_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "Visual Studio Code", "dev_vscode",
        [vscode_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(vscode_menu);
            return true;
        }));

    // 2 — Android Studio → ide 子菜单 type 2 (install/delete pkg/remove)
    auto ide2_menu = build_ide_submenu_02(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "Android Studio", "dev_android_studio",
        [mgr, ide2_menu](MenuContext& ctx) -> bool {
            mgr->prep_android_studio();
            MenuEngine(ctx).run(ide2_menu);
            return true;
        }));

    // 3 — IntelliJ IDEA → 旗舰版/社区版选择 → ide 子菜单 type 1
    auto ide1_menu_idea = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "IntelliJ IDEA", "dev_idea",
        [mgr, ide1_menu_idea](MenuContext& ctx) -> bool {
            mgr->choose_idea_edition();
            MenuEngine(ctx).run(ide1_menu_idea);
            return true;
        }));

    // 4 — PyCharm Community → ide 子菜单 type 1
    auto ide1_menu_pycharm = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "PyCharm", "dev_pycharm",
        [mgr, ide1_menu_pycharm](MenuContext& ctx) -> bool {
            mgr->prep_pycharm();
            MenuEngine(ctx).run(ide1_menu_pycharm);
            return true;
        }));

    // 5 — WebStorm → ide 子菜单 type 1
    auto ide1_menu_webstorm = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "WebStorm", "dev_webstorm",
        [mgr, ide1_menu_webstorm](MenuContext& ctx) -> bool {
            mgr->prep_webstorm();
            MenuEngine(ctx).run(ide1_menu_webstorm);
            return true;
        }));

    // 6 — CLion → ide 子菜单 type 1
    auto ide1_menu_clion = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "CLion", "dev_clion",
        [mgr, ide1_menu_clion](MenuContext& ctx) -> bool {
            mgr->prep_clion();
            MenuEngine(ctx).run(ide1_menu_clion);
            return true;
        }));

    // 7 — GoLand → ide 子菜单 type 1
    auto ide1_menu_goland = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "GoLand", "dev_goland",
        [mgr, ide1_menu_goland](MenuContext& ctx) -> bool {
            mgr->prep_goland();
            MenuEngine(ctx).run(ide1_menu_goland);
            return true;
        }));

    // 8 — GNU Emacs → 包管理器快速安装
    menu->add_child(LambdaAction::make(
        "GNU Emacs", "dev_emacs",
        [mgr] { mgr->install_emacs(); Logger::press_enter(); }));

    // 9 — Code::Blocks → 包管理器快速安装
    menu->add_child(LambdaAction::make(
        "Code::Blocks", "dev_codeblocks",
        [mgr] { mgr->install_code_blocks(); Logger::press_enter(); }));

    // 10 — GitHub Desktop → ide 子菜单 type 1 (x64 only)
    auto ide1_menu_github = build_ide_submenu_01(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        "GitHub Desktop", "dev_github_desktop",
        [mgr, ide1_menu_github](MenuContext& ctx) -> bool {
            mgr->prep_github_desktop();
            MenuEngine(ctx).run(ide1_menu_github);
            return true;
        }));

    // 11 — Sublime Text → 添加官方源 + 安装
    menu->add_child(LambdaAction::make(
        "Sublime Text", "dev_sublime",
        [mgr] { mgr->install_sublime_text(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
