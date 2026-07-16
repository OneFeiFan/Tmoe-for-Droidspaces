/** Config 菜单插件 — 每个旧菜单项拆分为独立 LambdaAction，
 *  直接调用 ConfigManager 的对应领域方法。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/config_manager.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_config_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.config"), _("config.menu_prompt"), "plugin_config");

    // 1 — DNS
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.dns"), "cfg_dns",
        [mgr](MenuContext&) -> bool {
            mgr->configure_dns();
            Logger::press_enter();
            return true;
        }));

    // 2 — 时区
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.timezone"), "cfg_timezone",
        [mgr](MenuContext&) -> bool {
            mgr->configure_timezone();
            Logger::press_enter();
            return true;
        }));

    // 3 — Locale
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.locale"), "cfg_locale",
        [mgr](MenuContext&) -> bool {
            mgr->configure_locale();
            Logger::press_enter();
            return true;
        }));

    // 4 — Fortune / Hitokoto
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.fortune"), "cfg_fortune",
        [mgr](MenuContext&) -> bool {
            mgr->configure_fortune();
            Logger::press_enter();
            return true;
        }));

    // 5 — 共享目录 (内部已调用 Logger::press_enter())
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.shared_dirs"), "cfg_shared_dirs",
        [mgr](MenuContext&) -> bool {
            mgr->configure_shared_dirs();
            return true;
        }));

    // 6 — Root 密码
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.password"), "cfg_password",
        [mgr](MenuContext&) -> bool {
            mgr->change_root_password();
            Logger::press_enter();
            return true;
        }));

    // 7 — 主机名
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.hostname"), "cfg_hostname",
        [mgr](MenuContext&) -> bool {
            mgr->configure_hostname();
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
