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
        _("office.libreoffice"), "install_libreoffice",
        [this] { mgr_->install_libreoffice(false); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.libreoffice_zh"), "install_libreoffice_zh",
        [this] { mgr_->install_libreoffice(true); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.wps"), "install_wps",
        [this] { mgr_->install_wps(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.yozo"), "install_yozo",
        [this] { mgr_->install_yozo(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.freeoffice"), "install_freeoffice",
        [this] { mgr_->install_freeoffice(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.meld"), "install_meld",
        [this] { mgr_->install_meld(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.kdiff3"), "install_kdiff3",
        [this] { mgr_->install_kdiff3(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.manpages_zh"), "install_manpages_zh",
        [this] { mgr_->install_manpages_zh(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
