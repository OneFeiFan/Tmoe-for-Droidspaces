/** DevTools 菜单插件 — 每个 IDE/工具作为独立的 LambdaAction 或嵌套子菜单。
 *  VS Code / IDE (type1/type2) 不再调用旧的 whiptail 子菜单，
 *  而是构建 SimpleMenu + LambdaAction 子菜单树。 */
#include "ui/menus/dev_tools_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/apps/dev_tools.h"

namespace tmoe::ui::menus {

// ── 顶层入口 ──────────────────────────────────────────────

std::shared_ptr<IUIMenu> DevToolsMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("devtools.menu_title"), _("devtools.tightvnc_tip"), "plugin_devtools");

    // 1 — Visual Studio Code → vscode 子菜单 (Official/Server/Codium/fix)
    menu->add_submenu("Visual Studio Code", "dev_vscode", build_vscode_menu());

    // 2 — Android Studio → ide 子菜单 type 2 (install/delete pkg/remove)
    auto ide2_menu = build_ide_submenu_02();
    menu->add_child(std::make_shared<LambdaAction>(
        "Android Studio", "dev_android_studio",
        [this, ide2_menu](MenuContext& ctx) -> bool {
            mgr_->prep_android_studio();
            MenuEngine(ctx).run(ide2_menu);
            return true;
        }));

    // 3 — IntelliJ IDEA → 旗舰版/社区版选择 → ide 子菜单 type 1
    auto ide1_menu_idea = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "IntelliJ IDEA", "dev_idea",
        [this, ide1_menu_idea](MenuContext& ctx) -> bool {
            mgr_->choose_idea_edition();
            MenuEngine(ctx).run(ide1_menu_idea);
            return true;
        }));

    // 4 — PyCharm Community → ide 子菜单 type 1
    auto ide1_menu_pycharm = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "PyCharm", "dev_pycharm",
        [this, ide1_menu_pycharm](MenuContext& ctx) -> bool {
            mgr_->prep_pycharm();
            MenuEngine(ctx).run(ide1_menu_pycharm);
            return true;
        }));

    // 5 — WebStorm → ide 子菜单 type 1
    auto ide1_menu_webstorm = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "WebStorm", "dev_webstorm",
        [this, ide1_menu_webstorm](MenuContext& ctx) -> bool {
            mgr_->prep_webstorm();
            MenuEngine(ctx).run(ide1_menu_webstorm);
            return true;
        }));

    // 6 — CLion → ide 子菜单 type 1
    auto ide1_menu_clion = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "CLion", "dev_clion",
        [this, ide1_menu_clion](MenuContext& ctx) -> bool {
            mgr_->prep_clion();
            MenuEngine(ctx).run(ide1_menu_clion);
            return true;
        }));

    // 7 — GoLand → ide 子菜单 type 1
    auto ide1_menu_goland = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "GoLand", "dev_goland",
        [this, ide1_menu_goland](MenuContext& ctx) -> bool {
            mgr_->prep_goland();
            MenuEngine(ctx).run(ide1_menu_goland);
            return true;
        }));

    // 8 — GNU Emacs → 包管理器快速安装
    menu->add_action("GNU Emacs", "dev_emacs",
        [this] { mgr_->install_emacs(); });

    // 9 — Code::Blocks → 包管理器快速安装
    menu->add_action("Code::Blocks", "dev_codeblocks",
        [this] { mgr_->install_code_blocks(); });

    // 10 — GitHub Desktop → ide 子菜单 type 1 (x64 only)
    auto ide1_menu_github = build_ide_submenu_01();
    menu->add_child(std::make_shared<LambdaAction>(
        "GitHub Desktop", "dev_github_desktop",
        [this, ide1_menu_github](MenuContext& ctx) -> bool {
            mgr_->prep_github_desktop();
            MenuEngine(ctx).run(ide1_menu_github);
            return true;
        }));

    // 11 — Sublime Text → 添加官方源 + 安装
    menu->add_action("Sublime Text", "dev_sublime",
        [this] { mgr_->install_sublime_text(); });

    add_sandwich_nav(menu);
    return menu;
}

// ── VS Code 子菜单 ─────────────────────────────────────

std::shared_ptr<IUIMenu> DevToolsMenuPlugin::build_vscode_menu() {
    auto menu = make_plugin_menu(
        "Visual Studio Code", _("devtools.vscode_tip"), "plugin_dev_vscode");

    menu->add_action("Microsoft Official", "1",
        [this] { mgr_->install_vscode_official(); });
    menu->add_action("VS Code Server", "2",
        [this] { mgr_->run_vscode_server_menu(); });
    menu->add_action("VS Codium", "3",
        [this] { mgr_->install_vscodium(); });
    menu->add_action("Fix tightvnc vscode", "4",
        [this] { mgr_->fix_tightvnc_vscode(); });

    add_navigation_items(menu);
    return menu;
}

// ── IDE 子菜单 type 1 (JetBrains / GitHub Desktop) ──────

std::shared_ptr<IUIMenu> DevToolsMenuPlugin::build_ide_submenu_01() {
    auto menu = make_plugin_menu(
        "IDE Manager", "Install / Delete / Remove", "plugin_dev_ide_01");

    menu->add_action("Install/Upgrade", "1",
        [this] { mgr_->install_ide_01(); });
    menu->add_action("Delete Package", "2",
        [this] { mgr_->delete_ide_pkg(); });
    menu->add_action("Remove", "3",
        [this] { mgr_->remove_ide_01(); });

    add_navigation_items(menu);
    return menu;
}

// ── IDE 子菜单 type 2 (Android Studio) ─────────────────

std::shared_ptr<IUIMenu> DevToolsMenuPlugin::build_ide_submenu_02() {
    auto menu = make_plugin_menu(
        "IDE Manager", "Install / Delete / Remove", "plugin_dev_ide_02");

    menu->add_action("Install/Upgrade", "1",
        [this] { mgr_->install_ide_02(); });
    menu->add_action("Delete Package", "2",
        [this] { mgr_->delete_ide_pkg_02(); });
    menu->add_action("Remove", "3",
        [this] { mgr_->remove_ide_02(); });

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
