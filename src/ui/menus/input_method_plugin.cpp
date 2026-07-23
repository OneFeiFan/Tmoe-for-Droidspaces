/** InputMethod 菜单插件 — 每个输入法框架展开为独立的嵌套子菜单。
 *  fcitx4/fcitx5/ibus 不再调用旧的 whiptail 子菜单，而是构建
 *  SimpleMenu + LambdaAction 子菜单树。 */
#include "ui/menus/input_method_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/apps/input_method.h"

namespace tmoe::ui::menus {

// ── 顶层入口 ──────────────────────────────────────────────

    std::shared_ptr<IUIMenu> InputMethodMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("input.menu_title"), _("input.menu_prompt"), "plugin_input_method");

        // Fcitx4 — 嵌套子菜单
        menu->add_submenu(_("input.fcitx4"), "input_fcitx4", build_fcitx4_menu());

        // Fcitx5 — 嵌套子菜单
        menu->add_submenu(_("input.fcitx5"), "input_fcitx5", build_fcitx5_menu());

        // IBus — 嵌套子菜单
        menu->add_submenu(_("input.ibus"), "input_ibus", build_ibus_menu());

        // Sogou — 直接操作
        menu->add_action(_("input.sogou"), "input_sogou",
                         [this] { mgr_->install_sogou(); });

        // FAQ — 直接操作
        menu->add_action(_("input.faq"), "input_faq",
                         [this] { mgr_->show_input_faq(); });

        add_sandwich_nav(menu);
        return menu;
    }

// ── fcitx4 子菜单 ─────────────────────────────────────────

    std::shared_ptr<IUIMenu> InputMethodMenuPlugin::build_fcitx4_menu() {
        auto menu = make_plugin_menu(
                _("input.fcitx4_menu"), "", "plugin_input_fcitx4");

        menu->add_action(_("input.fcitx4_googlepinyin"), "1",
                         [this] { mgr_->install_fcitx4_engine("fcitx-googlepinyin"); });
        menu->add_action(_("input.fcitx4_rime"), "2",
                         [this] { mgr_->install_fcitx4_engine("fcitx-rime"); });
        menu->add_action(_("input.fcitx4_faq"), "3",
                         [this] { mgr_->show_input_faq(); });
        menu->add_action(_("input.fcitx4_libpinyin"), "4",
                         [this] { mgr_->install_fcitx4_engine("fcitx-libpinyin"); });
        menu->add_action(_("input.fcitx4_sunpinyin"), "5",
                         [this] { mgr_->install_fcitx4_engine("fcitx-sunpinyin"); });
        menu->add_action(_("input.fcitx4_cloudpinyin"), "6",
                         [this] { mgr_->install_fcitx4_engine("fcitx-module-cloudpinyin"); });
        menu->add_action(_("input.fcitx4_kde_config"), "7",
                         [this] { mgr_->install_fcitx4_engine("kde-config-fcitx"); });
        menu->add_action(_("input.fcitx4_tools"), "8",
                         [this] { mgr_->install_fcitx4_tools(); });

        add_navigation_items(menu);
        return menu;
    }

// ── fcitx5 子菜单 ─────────────────────────────────────────

    std::shared_ptr<IUIMenu> InputMethodMenuPlugin::build_fcitx5_menu() {
        auto menu = make_plugin_menu(
                _("input.fcitx5_menu"), "", "plugin_input_fcitx5");

        menu->add_action(_("input.fcitx5_core"), "1",
                         [this] { mgr_->install_fcitx5_packages({"fcitx5", "fcitx5-chinese-addons"}); });
        menu->add_action(_("input.fcitx5_qt_gtk"), "2",
                         [this] { mgr_->install_fcitx5_packages({"fcitx5-qt", "fcitx5-gtk"}); });
        menu->add_action(_("input.fcitx5_kcm"), "3",
                         [this] { mgr_->install_fcitx5_packages({"kcm-fcitx5"}); });
        menu->add_action(_("input.fcitx5_rime"), "4",
                         [this] { mgr_->install_fcitx5_packages({"fcitx5-rime"}); });
        menu->add_action(_("input.fcitx5_material"), "5",
                         [this] { mgr_->install_fcitx5_packages({"fcitx5-material-color"}); });
        menu->add_action(_("input.fcitx5_kimpanel"), "6",
                         [this] {
                             mgr_->install_fcitx5_packages(
                                     {"fcitx5-module-kimpanel", "gnome-shell-extension-kimpanel"});
                         });

        add_navigation_items(menu);
        return menu;
    }

// ── ibus 子菜单 ───────────────────────────────────────────

    std::shared_ptr<IUIMenu> InputMethodMenuPlugin::build_ibus_menu() {
        auto menu = make_plugin_menu(
                _("input.ibus_menu"), "", "plugin_input_ibus");

        menu->add_action(_("input.ibus_pinyin"), "1",
                         [this] { mgr_->install_ibus_engine("ibus-libpinyin"); });
        menu->add_action(_("input.ibus_rime"), "2",
                         [this] { mgr_->install_ibus_engine("ibus-rime"); });
        menu->add_action(_("input.ibus_faq"), "3",
                         [this] { mgr_->show_input_faq(); });
        menu->add_action(_("input.ibus_sunpinyin"), "4",
                         [this] { mgr_->install_ibus_engine("ibus-sunpinyin"); });
        menu->add_action(_("input.ibus_chewing"), "5",
                         [this] { mgr_->install_ibus_engine("ibus-chewing"); });

        add_navigation_items(menu);
        return menu;
    }

} // namespace tmoe::ui::menus
