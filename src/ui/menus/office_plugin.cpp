/** Office 菜单插件 — 将 run_office_menu() 的各个选项拆分为独立 LambdaAction。 */
#include "ui/menus/office_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/office.h"

namespace tmoe::ui::menus {

OfficeMenuPlugin::OfficeMenuPlugin(domain::OfficeManager* mgr) : mgr_(mgr) {}

std::shared_ptr<IUIMenu> OfficeMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("office.menu_title"), _("office.menu_prompt"), "plugin_office");

    menu->add_child(LambdaAction::make(
        _("office.libreoffice"), "1",
        [this] { mgr_->install_libreoffice(false); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.libreoffice_zh"), "2",
        [this] { mgr_->install_libreoffice(true); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.wps"), "3",
        [this] { mgr_->wps.install(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.yozo"), "4",
        [this] { mgr_->yozo.install(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.freeoffice"), "5",
        [this] { mgr_->freeoffice.install(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.meld"), "6",
        [this] { mgr_->meld.install(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.kdiff3"), "7",
        [this] { mgr_->kdiff3.install(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.manpages_zh"), "8",
        [this] { mgr_->manpages_zh.install(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
