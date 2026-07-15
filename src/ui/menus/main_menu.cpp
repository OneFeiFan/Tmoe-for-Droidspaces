#include "main_menu.h"
#include "ui/delegate_actions.h"
#include "ui/builtin_actions.h"
#include "ui/registry.h"
#include "core/i18n.h"
#include "core/config.h"

namespace tmoe::ui::menus {

std::vector<std::shared_ptr<IMenuItem>> MenuBuilder::make_items(
    const MenuBuilder::RouteMap& routes,
    const std::vector<std::pair<std::string, std::string>>& key_labels)
{
    std::vector<std::shared_ptr<IMenuItem>> items;
    for (const auto& [key, label] : key_labels) {
        auto it = routes.find(key);
        if (it != routes.end()) {
            items.push_back(std::make_shared<DelegateAction>(
                label, key, it->second));
        }
    }
    return items;
}

std::shared_ptr<IUIMenu> MenuBuilder::build_main_menu(
    const std::string& title,
    const RouteMap& routes)
{
    auto menu = std::make_shared<SimpleMenu>(
        title, _("menu.tui.title"), "main_menu");

    menu->add_children(make_items(routes, {
        {"1",  _("menu.tui.proot")},
        {"2",  _("menu.tui.chroot")},
        {"3",  _("menu.tui.remove")},
        {"4",  _("menu.tui.locale")},
        {"5",  _("menu.tui.termux")},
        {"6",  _("menu.tui.zsh")},
        {"7",  _("menu.tui.update")},
        {"8",  _("menu.tui.faq")},
        {"9",  _("menu.tui.report")},
        {"10", _("menu.tui.fix_signal9")},
        {"11", _("menu.tui.mirrors")},
        {"12", _("menu.tui.gui_de")},
        {"13", _("menu.tui.gui_remote")},
        {"14", _("menu.tui.gui_beautify")},
        {"15", _("menu.tui.backup")},
        {"16", _("menu.tui.secret_garden")},
        {"17", _("menu.tui.config")},
        {"18", _("menu.tui.software_center")},
    }));

    add_navigation_items(menu);
    return menu;
}

std::shared_ptr<IUIMenu> MenuBuilder::build_tool_menu(
    const std::string& title,
    const RouteMap& routes)
{
    auto menu = std::make_shared<SimpleMenu>(
        title, _("menu.tui.tool_prompt"), "tool_menu");

    // 工具菜单重新映射：屏幕编号 → routes key
    menu->add_children(make_items(routes, {
        {"12", _("menu.tui.gui_de")},
        {"18", _("menu.tui.software_center")},
        {"16", _("menu.tui.secret_garden")},
        {"14", _("menu.tui.gui_beautify")},
        {"13", _("menu.tui.gui_remote")},
        {"11", _("menu.tui.mirrors")},
        {"8",  _("menu.tui.faq")},
    }));

    add_navigation_items(menu);
    return menu;
}

std::shared_ptr<IUIMenu> MenuBuilder::build_manager_menu(
    const std::string& title,
    const RouteMap& routes)
{
    auto menu = std::make_shared<SimpleMenu>(
        title, _("menu.tui.manager_prompt"), "manager_menu");

    menu->add_children(make_items(routes, {
        {"1",  _("menu.tui.proot")},
        {"2",  _("menu.tui.chroot")},
        {"3",  _("menu.tui.remove")},
        {"4",  _("menu.tui.locale")},
        {"5",  _("menu.tui.termux")},
        {"6",  _("menu.tui.zsh")},
        {"7",  _("menu.tui.update")},
        {"8",  _("menu.tui.faq")},
        {"9",  _("menu.tui.report")},
        {"10", _("menu.tui.fix_signal9")},
    }));

    add_navigation_items(menu);
    return menu;
}

std::shared_ptr<IUIMenu> MenuBuilder::build_from_registry(
    const std::string& title)
{
    auto menu = std::make_shared<SimpleMenu>(
        title, _("menu.tui.title"), "plugin_main");

    // 从全局注册表收集所有已注册插件
    auto plugins = MenuRegistry::items();
    for (auto& item : plugins) {
        if (item) {
            menu->add_child(item);
        }
    }

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
