/** Browser 菜单插件 — TUI 交互逻辑在插件层，领域层只做业务（安装/卸载）。 */
#include "ui/menus/browser_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/dialog_helpers.h"
#include "ui/choice_action.h"
#include "domain/apps/browser.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> BrowserMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser");

    // ── Firefox / Chromium — 多步向导 ──
    menu->add_action(_("browser.menu_item_firefox_chromium"), "1",
        [this] {
            int c = dialog::yesno(mgr_->cfg(),
                _("browser.firefox_chromium_select_title"),
                _("browser.firefox_chromium_select_yesno"),
                "chromium", "Firefox", 15, 50);
            if (c == 0) {
                int c2 = dialog::yesno(mgr_->cfg(),
                    _("browser.chromium_install_remove_title"),
                    _("browser.chromium_install_remove_yesno"),
                    "install", "remove", 8, 50);
                if (c2 == 0) mgr_->chromium.install();
                else if (c2 == 1) mgr_->chromium.remove();
            } else if (c == 1) {
                int c2 = dialog::yesno(mgr_->cfg(),
                    _("browser.firefox_install_remove_title"),
                    _("browser.firefox_install_remove_yesno"),
                    "install", "remove", 8, 50);
                if (c2 == 0 || c2 == 1) {
                    int c3 = dialog::yesno(mgr_->cfg(),
                        _("browser.firefox_chromium_select_title"),
                        _("browser.firefox_esr_select_yesno"),
                        "Firefox", "ESR", 12, 53);
                    if (c2 == 0) {
                        if (c3 == 0) mgr_->firefox.install();
                        else if (c3 == 1) mgr_->install_firefox_esr();
                    } else {
                        if (c3 == 0) mgr_->firefox.remove();
                        else if (c3 == 1) mgr_->firefox_esr.remove();
                    }
                }
            }
        });

    // ── Microsoft Edge — 安装/卸载双按钮 ──
    menu->add_child(std::make_shared<ChoiceAction>(
        _("browser.menu_item_edge"), "2",
        _("browser.edge_install_remove_title"), _("browser.edge_install_remove_yesno"),
        "install", [this] { mgr_->edge.install(); },
        "remove",  [this] { mgr_->edge.remove(); },
        10, 50));

    // ── Falkon — 安装/卸载双按钮 ──
    menu->add_child(std::make_shared<ChoiceAction>(
        _("browser.menu_item_falkon"), "3",
        _("browser.falkon_install_remove_title"), _("browser.falkon_install_remove_yesno"),
        "install", [this] { mgr_->falkon.install(); },
        "remove",  [this] { mgr_->falkon.remove(); },
        8, 50));

    // ── 仅安装的应用 ──
    menu->add_action(_("browser.menu_item_vivaldi"), "4",
        [this] { mgr_->vivaldi.install(); });

    menu->add_action(_("browser.menu_item_epiphany"), "5",
        [this] { mgr_->epiphany.install(); });

    menu->add_action(_("browser.menu_item_midori"), "6",
        [this] { mgr_->midori.install(); });

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
