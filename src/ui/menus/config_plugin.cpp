/** Config 菜单插件 — 将旧版 whiptail 子菜单（DNS/时区/Locale/Fortune）
 *  迁移至 IUIMenu 嵌套引擎框架。
 *  configure_shared_dirs (checklist)、configure_hostname (inputbox)、
 *  change_root_password (passwordbox) 保持旧版，不迁移。 */

#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/system/config_manager.h"
#include "domain/system/environment.h"

namespace tmoe::ui::menus {

// ── DNS 子菜单 ──────────────────────────────────────────

static std::shared_ptr<IUIMenu> build_dns_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(_("dns.title"), _("dns.menu_prompt"), "plugin_dns");
    for (const auto& dns : mgr->dns_registry()) {
        menu->add_child(std::make_shared<LambdaAction>(
            _(dns.name_key) + " (" + dns.primary + ")", dns.id,
            [mgr, id = dns.id](MenuContext&) -> bool {
                mgr->apply_dns(id);
                Logger::press_enter();
                return true;
            }));
    }
    add_sandwich_nav(menu);
    return menu;
}

// ── 时区子菜单（两层） ──────────────────────────────────

static std::shared_ptr<IUIMenu> build_timezone_city_menu(
    domain::ConfigManager* mgr, int region_index);

static std::shared_ptr<IUIMenu> build_timezone_region_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(_("tz.title"), _("tz.select_region"), "plugin_tz_region");
    const auto& regions = mgr->tz_registry();
    for (size_t i = 0; i < regions.size(); ++i) {
        menu->add_child(std::make_shared<LambdaAction>(
            _(regions[i].first), std::to_string(i + 1),
            [mgr, ri = static_cast<int>(i)](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(build_timezone_city_menu(mgr, ri));
                return true;
            }));
    }
    add_sandwich_nav(menu);
    return menu;
}

static std::shared_ptr<IUIMenu> build_timezone_city_menu(
    domain::ConfigManager* mgr, int region_index) {
    const auto& regions = mgr->tz_registry();
    auto menu = make_plugin_menu(
        _(regions[region_index].first), _("tz.select_city"), "plugin_tz_city");
    for (size_t j = 0; j < regions[region_index].second.size(); ++j) {
        const auto& city = regions[region_index].second[j];
        menu->add_child(std::make_shared<LambdaAction>(
            _(city.name_key) + " (" + city.zone + ")", city.zone,
            [mgr, zone = city.zone](MenuContext&) -> bool {
                mgr->apply_timezone(zone);
                Logger::press_enter();
                return true;
            }));
    }
    add_sandwich_nav(menu);
    return menu;
}

// ── Locale 子菜单（两层） ────────────────────────────────

static std::shared_ptr<IUIMenu> build_locale_select_menu(
    domain::ConfigManager* mgr, int region_index);

static std::shared_ptr<IUIMenu> build_locale_region_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(
        _("locale.title"), _("locale.select_region"), "plugin_locale_region");
    const auto& regions = mgr->locale_registry();
    for (size_t i = 0; i < regions.size(); ++i) {
        menu->add_child(std::make_shared<LambdaAction>(
            _(regions[i].first), std::to_string(i + 1),
            [mgr, ri = static_cast<int>(i)](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(build_locale_select_menu(mgr, ri));
                return true;
            }));
    }
    add_sandwich_nav(menu);
    return menu;
}

static std::shared_ptr<IUIMenu> build_locale_select_menu(
    domain::ConfigManager* mgr, int region_index) {
    const auto& regions = mgr->locale_registry();
    auto menu = make_plugin_menu(
        _(regions[region_index].first), _("locale.select_prompt"), "plugin_locale_select");
    for (size_t j = 0; j < regions[region_index].second.size(); ++j) {
        const auto& loc = regions[region_index].second[j];
        menu->add_child(std::make_shared<LambdaAction>(
            loc, loc,
            [mgr, loc](MenuContext&) -> bool {
                Logger::step(_f("locale.applying", loc));
                // 写入 tmoe 持久化配置
                mgr->write_config_file(mgr->config_dir() + "/locale.txt", loc);
                // 调用 Environment 进行系统级设置
                domain::Environment env(mgr->config());
                bool ok = env.set_locale(loc);
                if (ok) ok = env.apply_system_locale(loc);
                if (ok) {
                    Logger::ok(_f("locale.applied", loc));
                } else {
                    Logger::error(_("locale.failed"));
                }
                Logger::press_enter();
                return ok;
            }));
    }
    add_sandwich_nav(menu);
    return menu;
}

// ── Fortune 子菜单 ──────────────────────────────────────

static std::shared_ptr<IUIMenu> build_fortune_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(_("fortune.title"), _("fortune.menu_prompt"), "plugin_fortune");

    menu->add_child(std::make_shared<LambdaAction>(
        _("fortune.install"), "1",
        [mgr](MenuContext&) -> bool {
            bool ok = mgr->install_fortune();
            Logger::press_enter();
            return ok;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("fortune.hitokoto_enable"), "2",
        [mgr](MenuContext&) -> bool {
            bool ok = mgr->toggle_hitokoto();
            Logger::press_enter();
            return ok;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("fortune.hitokoto_disable"), "3",
        [mgr](MenuContext&) -> bool {
            mgr->write_config_file(
                mgr->config_dir() + "/hitokoto.conf", "TMOE_CONTAINER_HITOKOTO=false");
            Logger::ok(_("fortune.hitokoto_disabled"));
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── 主配置菜单入口 ──────────────────────────────────────

std::shared_ptr<IUIMenu> create_config_menu(domain::ConfigManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.config"), _("config.menu_prompt"), "plugin_config");

    // 1 — DNS → 嵌套引擎运行 DNS 子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.dns"), "cfg_dns",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_dns_menu(mgr));
            return true;
        }));

    // 2 — 时区 → 嵌套引擎运行两层时区菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.timezone"), "cfg_timezone",
        [mgr](MenuContext& ctx) -> bool {
            // 显示当前时区提示
            std::string current = mgr->detect_current_timezone();
            Logger::info(_("tz.current") + ": " + current);
            MenuEngine(ctx).run(build_timezone_region_menu(mgr));
            return true;
        }));

    // 3 — Locale → 嵌套引擎运行两层 Locale 菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.locale"), "cfg_locale",
        [mgr](MenuContext& ctx) -> bool {
            // 显示当前 locale 提示
            std::string current_locale = "en_US.UTF-8";
            const char* env_lang = std::getenv("LANG");
            if (env_lang) current_locale = env_lang;
            Logger::info(_("locale.current") + ": " + current_locale);
            MenuEngine(ctx).run(build_locale_region_menu(mgr));
            return true;
        }));

    // 4 — Fortune / Hitokoto → 嵌套引擎运行 Fortune 子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.fortune"), "cfg_fortune",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_fortune_menu(mgr));
            return true;
        }));

    // 5 — 共享目录 (内部已调用 Logger::press_enter())
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.shared_dirs"), "cfg_shared_dirs",
        [mgr](MenuContext&) -> bool {
            mgr->configure_shared_dirs();
            return true;
        }));

    // 6 — 修改 root 密码
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
