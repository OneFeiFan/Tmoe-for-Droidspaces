/** InputMethod 菜单插件 — 每个输入法框架展开为独立的嵌套子菜单。
 *  fcitx4/fcitx5/ibus 不再调用旧的 whiptail 子菜单，而是构建
 *  SimpleMenu + LambdaAction 子菜单树。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/apps/input_method.h"

namespace tmoe::ui::menus {

// ── fcitx4 子菜单 ─────────────────────────────────────────

static std::shared_ptr<IUIMenu> build_fcitx4_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.fcitx4_menu"), "", "plugin_input_fcitx4");

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_googlepinyin"), "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("fcitx-googlepinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_rime"), "2",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("fcitx-rime");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_faq"), "3",
        [mgr](MenuContext&) -> bool {
            mgr->show_input_faq();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_libpinyin"), "4",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("fcitx-libpinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_sunpinyin"), "5",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("fcitx-sunpinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_cloudpinyin"), "6",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("fcitx-module-cloudpinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_kde_config"), "7",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_engine("kde-config-fcitx");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4_tools"), "8",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx4_tools();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── fcitx5 子菜单 ─────────────────────────────────────────

static std::shared_ptr<IUIMenu> build_fcitx5_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.fcitx5_menu"), "", "plugin_input_fcitx5");

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_core"), "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"fcitx5", "fcitx5-chinese-addons"});
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_qt_gtk"), "2",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"fcitx5-qt", "fcitx5-gtk"});
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_kcm"), "3",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"kcm-fcitx5"});
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_rime"), "4",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"fcitx5-rime"});
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_material"), "5",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"fcitx5-material-color"});
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5_kimpanel"), "6",
        [mgr](MenuContext&) -> bool {
            mgr->install_fcitx5_packages({"fcitx5-module-kimpanel", "gnome-shell-extension-kimpanel"});
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── ibus 子菜单 ───────────────────────────────────────────

static std::shared_ptr<IUIMenu> build_ibus_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.ibus_menu"), "", "plugin_input_ibus");

    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus_pinyin"), "1",
        [mgr](MenuContext&) -> bool {
            mgr->install_ibus_engine("ibus-libpinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus_rime"), "2",
        [mgr](MenuContext&) -> bool {
            mgr->install_ibus_engine("ibus-rime");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus_faq"), "3",
        [mgr](MenuContext&) -> bool {
            mgr->show_input_faq();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus_sunpinyin"), "4",
        [mgr](MenuContext&) -> bool {
            mgr->install_ibus_engine("ibus-sunpinyin");
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus_chewing"), "5",
        [mgr](MenuContext&) -> bool {
            mgr->install_ibus_engine("ibus-chewing");
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── 顶层入口 ──────────────────────────────────────────────

std::shared_ptr<IUIMenu> create_input_method_menu(domain::InputMethodManager* mgr) {
    auto menu = make_plugin_menu(
        _("input.menu_title"), _("input.menu_prompt"), "plugin_input_method");

    // Fcitx4 — 嵌套子菜单
    auto fcitx4_menu = build_fcitx4_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx4"), "input_fcitx4",
        [fcitx4_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(fcitx4_menu);
            return true;
        }));

    // Fcitx5 — 嵌套子菜单
    auto fcitx5_menu = build_fcitx5_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.fcitx5"), "input_fcitx5",
        [fcitx5_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(fcitx5_menu);
            return true;
        }));

    // IBus — 嵌套子菜单
    auto ibus_menu = build_ibus_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("input.ibus"), "input_ibus",
        [ibus_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(ibus_menu);
            return true;
        }));

    // Sogou — 直接操作
    menu->add_child(LambdaAction::make(
        _("input.sogou"), "input_sogou",
        [mgr] { mgr->install_sogou(); Logger::press_enter(); }));

    // FAQ — 直接操作
    menu->add_child(LambdaAction::make(
        _("input.faq"), "input_faq",
        [mgr] { mgr->show_input_faq(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
