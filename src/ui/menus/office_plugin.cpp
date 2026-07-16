/** Office 菜单插件 — 将 run_office_menu() 的各个选项拆分为独立 LambdaAction。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/office.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_office_menu(domain::OfficeManager* mgr) {
    auto menu = make_plugin_menu(
        _("office.menu_title"), _("office.menu_prompt"), "plugin_office");

    menu->add_child(LambdaAction::make(
        _("office.libreoffice"), "install_libreoffice",
        [mgr] { mgr->install_libreoffice(false); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.libreoffice_zh"), "install_libreoffice_zh",
        [mgr] { mgr->install_libreoffice(true); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.wps"), "install_wps",
        [mgr] { mgr->install_wps(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.yozo"), "install_yozo",
        [mgr] { mgr->install_yozo(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.freeoffice"), "install_freeoffice",
        [mgr] { mgr->install_freeoffice(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.meld"), "install_meld",
        [mgr] { mgr->install_meld(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.kdiff3"), "install_kdiff3",
        [mgr] { mgr->install_kdiff3(); Logger::press_enter(); }));

    menu->add_child(LambdaAction::make(
        _("office.manpages_zh"), "install_manpages_zh",
        [mgr] { mgr->install_manpages_zh(); Logger::press_enter(); }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
