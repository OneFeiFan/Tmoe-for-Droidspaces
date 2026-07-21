/** Config 菜单插件 — 将旧版 whiptail 子菜单（DNS/时区/Locale）
 *  迁移至 IUIMenu 嵌套引擎框架。
 *  configure_shared_dirs (checklist)、configure_hostname (inputbox)、
 *  change_root_password (passwordbox) 保持旧版，不迁移。 */

#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/system/config_manager.h"
#include "domain/system/environment.h"
#include "ui/menus/config_plugin.h"

namespace tmoe::ui::menus {

// ── DNS 子菜单 ──────────────────────────────────────────

std::shared_ptr<IUIMenu> ConfigMenuPlugin::build_dns_menu() {
    auto menu = make_plugin_menu(_("dns.title"), _("dns.menu_prompt"), "plugin_dns");
    for (const auto& dns : mgr_->dns_registry()) {
        menu->add_action(_(dns.name_key) + " (" + dns.primary + ")", dns.id,
            [this, id = dns.id] { mgr_->apply_dns(id); });
    }
    add_sandwich_nav(menu);
    return menu;
}

// ── 时区子菜单（两层） ──────────────────────────────────



std::shared_ptr<IUIMenu> ConfigMenuPlugin::build_timezone_region_menu() {
    auto menu = make_plugin_menu(_("tz.title"), _("tz.select_region"), "plugin_tz_region");
    const auto& regions = mgr_->tz_registry();
    for (size_t i = 0; i < regions.size(); ++i) {
        menu->add_submenu(_(regions[i].first), std::to_string(i + 1),
            build_timezone_city_menu(static_cast<int>(i)));
    }
    add_sandwich_nav(menu);
    return menu;
}

std::shared_ptr<IUIMenu> ConfigMenuPlugin::build_timezone_city_menu(int region_index) {
    const auto& regions = mgr_->tz_registry();
    auto menu = make_plugin_menu(
        _(regions[region_index].first), _("tz.select_city"), "plugin_tz_city");
    for (size_t j = 0; j < regions[region_index].second.size(); ++j) {
        const auto& city = regions[region_index].second[j];
        menu->add_action(_(city.name_key) + " (" + city.zone + ")", city.zone,
            [this, zone = city.zone] { mgr_->apply_timezone(zone); });
    }
    add_sandwich_nav(menu);
    return menu;
}

// ── Locale 子菜单（两层） ────────────────────────────────

std::shared_ptr<IUIMenu> ConfigMenuPlugin::build_locale_region_menu() {
    auto menu = make_plugin_menu(
        _("locale.title"), _("locale.select_region"), "plugin_locale_region");
    const auto& regions = mgr_->locale_registry();
    for (size_t i = 0; i < regions.size(); ++i) {
        menu->add_submenu(_(regions[i].first), std::to_string(i + 1),
            build_locale_select_menu(static_cast<int>(i)));
    }
    add_sandwich_nav(menu);
    return menu;
}

std::shared_ptr<IUIMenu> ConfigMenuPlugin::build_locale_select_menu(int region_index) {
    const auto& regions = mgr_->locale_registry();
    auto menu = make_plugin_menu(
        _(regions[region_index].first), _("locale.select_prompt"), "plugin_locale_select");
    for (size_t j = 0; j < regions[region_index].second.size(); ++j) {
        const auto& loc = regions[region_index].second[j];
        menu->add_child(std::make_shared<LambdaAction>(
            loc, loc,
            [this, loc](MenuContext&) -> bool {
                Logger::step(_f("locale.applying", loc));
                // 写入 tmoe 持久化配置
                mgr_->write_config_file(mgr_->config_dir() + "/locale.txt", loc);
                // 调用 Environment 进行系统级设置
                domain::Environment env(mgr_->config());
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

// ── 主配置菜单入口 ──────────────────────────────────────

std::shared_ptr<IUIMenu> ConfigMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("menu.tui.config"), _("config.menu_prompt"), "plugin_config");

    // 1 — DNS → 嵌套引擎运行 DNS 子菜单
    menu->add_submenu(_("config.dns"), "cfg_dns", build_dns_menu());

    // 2 — 时区 → 嵌套引擎运行两层时区菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.timezone"), "cfg_timezone",
        [this](MenuContext& ctx) -> bool {
            // 显示当前时区提示
            std::string current = mgr_->detect_current_timezone();
            Logger::info(_("tz.current") + ": " + current);
            MenuEngine(ctx).run(build_timezone_region_menu());
            return true;
        }));

    // 3 — Locale → 嵌套引擎运行两层 Locale 菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.locale"), "cfg_locale",
        [this](MenuContext& ctx) -> bool {
            // 显示当前 locale 提示
            std::string current_locale = "en_US.UTF-8";
            const char* env_lang = std::getenv("LANG");
            if (env_lang) current_locale = env_lang;
            Logger::info(_("locale.current") + ": " + current_locale);
            MenuEngine(ctx).run(build_locale_region_menu());
            return true;
        }));

    // 4 — 共享目录 (内部已调用 Logger::press_enter())
    menu->add_child(std::make_shared<LambdaAction>(
        _("config.shared_dirs"), "cfg_shared_dirs",
        [this](MenuContext&) -> bool {
            mgr_->configure_shared_dirs();
            return true;
        }));

    // 6 — 修改 root 密码
    menu->add_action(_("config.password"), "cfg_password",
        [this] { mgr_->change_root_password(); });

    // 7 — 主机名
    menu->add_action(_("config.hostname"), "cfg_hostname",
        [this] { mgr_->configure_hostname(); });

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus