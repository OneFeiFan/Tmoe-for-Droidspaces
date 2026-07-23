/** Office 菜单插件 — 将 run_office_menu() 的各个选项拆分为独立 LambdaAction。 */
#include "ui/menus/office_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/office.h"

namespace tmoe::ui::menus {

    std::shared_ptr<IUIMenu> OfficeMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("office.menu_title"), _("office.menu_prompt"), "plugin_office");

        menu->add_action(_("office.libreoffice"), "1",
                         [this] { mgr_->install_libreoffice(false); });

        menu->add_action(_("office.libreoffice_zh"), "2",
                         [this] { mgr_->install_libreoffice(true); });

        menu->add_action(_("office.wps"), "3",
                         [this] { mgr_->wps.install(); });

        menu->add_action(_("office.yozo"), "4",
                         [this] { mgr_->yozo.install(); });

        menu->add_action(_("office.freeoffice"), "5",
                         [this] { mgr_->freeoffice.install(); });

        menu->add_action(_("office.meld"), "6",
                         [this] { mgr_->meld.install(); });

        menu->add_action(_("office.kdiff3"), "7",
                         [this] { mgr_->kdiff3.install(); });

        menu->add_action(_("office.manpages_zh"), "8",
                         [this] { mgr_->manpages_zh.install(); });

        add_sandwich_nav(menu);
        return menu;
    }

} // namespace tmoe::ui::menus
