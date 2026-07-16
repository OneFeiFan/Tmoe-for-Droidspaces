/** GUI 桌面安装菜单插件 — 使用 IUIMenu 框架构建分层菜单。
 *
 *  菜单结构 (对应原 run_desktop_install_menu):
 *  ┌─ 顶层 SimpleMenu (add_navigation_items)
 *  │   ├─ 免 Root 桌面 (IUIMenu) → 动态 DE 列表 + sandwich nav
 *  │   ├─ 需 Root 桌面 (IUIMenu) → 动态 DE 列表 + sandwich nav
 *  │   ├─ 窗口管理器  (IUIMenu) → 动态 WM 列表 + sandwich nav
 *  │   └─ 显示管理器  (IUIMenu) → 固定 DM 列表 + sandwich nav
 *  │
 *  └─ 引擎自动分发: IUIMenu 进入子菜单, IAction 执行后返回上级
 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_desktop_menu(domain::GUIManager* gui) {
    auto menu = make_plugin_menu(
        _("gui.de_install_title"), _("menu.tui.title"), "plugin_gui_desktop");

    // ═══════════════════════════════════════════════════════════
    // 子菜单: 免 Root 桌面 (rootless DE)
    // ═══════════════════════════════════════════════════════════
    auto rootless_sub = make_plugin_menu(
        _("gui.de_rootless"), _("menu.tui.title"), "plugin_gui_de_rootless");
    for (const auto& d : gui->desktop_manager().desktop_registry()) {
        if (!d.requires_root && !d.is_window_manager) {
            rootless_sub->add_child(std::make_shared<LambdaAction>(
                d.name + " (" + d.compat_notes + ")",
                "de_install_" + d.id,
                [gui, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_desktop(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(rootless_sub);

    // ═══════════════════════════════════════════════════════════
    // 子菜单: 需 Root 桌面 (rootful DE)
    // ═══════════════════════════════════════════════════════════
    auto rootful_sub = make_plugin_menu(
        _("gui.de_rootful"), _("menu.tui.title"), "plugin_gui_de_rootful");
    for (const auto& d : gui->desktop_manager().desktop_registry()) {
        if (d.requires_root && !d.is_window_manager) {
            rootful_sub->add_child(std::make_shared<LambdaAction>(
                d.name + " (" + d.compat_notes + ")",
                "de_install_" + d.id,
                [gui, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_desktop(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(rootful_sub);

    // ═══════════════════════════════════════════════════════════
    // 子菜单: 窗口管理器 (WM)
    // ═══════════════════════════════════════════════════════════
    auto wm_sub = make_plugin_menu(
        _("gui.wm_title"), _("gui.wm_prompt"), "plugin_gui_de_wm");
    for (const auto& d : gui->desktop_manager().desktop_registry()) {
        if (d.is_window_manager) {
            auto wm_label = d.name
                + (d.compat_notes.empty() ? "" : " (" + d.compat_notes + ")");
            wm_sub->add_child(std::make_shared<LambdaAction>(
                wm_label,
                "wm_install_" + d.id,
                [gui, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_window_manager(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(wm_sub);

    // ═══════════════════════════════════════════════════════════
    // 子菜单: 显示管理器 (DM) — 5 个固定选项
    // ═══════════════════════════════════════════════════════════
    auto dm_sub = make_plugin_menu(
        _("gui.dm_title"), _("gui.dm_prompt"), "plugin_gui_de_dm");

    // LightDM
    dm_sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.lightdm"), "dm_install_lightdm",
        [gui](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            if (fam == DistroFamily::Alpine) {
                Executor::passthrough("sudo setup-xorg-base 2>/dev/null || true");
                domain::PackageManager::install(
                    {"lightdm", "lightdm-gtk-greeter",
                     "xf86-input-mouse", "xf86-input-keyboard",
                     "polkit", "consolekit2"}, fam);
            } else {
                domain::PackageManager::install(
                    {"lightdm", "lightdm-gtk-greeter",
                     "lightdm-gtk-greeter-settings", "ukui-greeter"}, fam);
            }
            gui->desktop_manager().tmoe_display_manager_systemctl(
                "lightdm", "lightdm");
            Logger::press_enter();
            return true;
        }));

    // SDDM
    dm_sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.sddm"), "dm_install_sddm",
        [gui](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"sddm", "sddm-theme-breeze"}, fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("sddm", "sddm");
            Logger::press_enter();
            return true;
        }));

    // GDM3
    dm_sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.gdm"), "dm_install_gdm",
        [gui](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"gdm", "gdm3"}, fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("gdm3", "gdm");
            Logger::press_enter();
            return true;
        }));

    // SLiM
    dm_sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.slim"), "dm_install_slim",
        [gui](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("slim", fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("slim", "slim");
            Logger::press_enter();
            return true;
        }));

    // LXDM
    dm_sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.lxdm"), "dm_install_lxdm",
        [gui](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("lxdm", fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("lxdm", "lxdm");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(dm_sub);

    // ═══════════════════════════════════════════════════════════
    // 顶层: 注册四个子菜单入口
    // ═══════════════════════════════════════════════════════════
    menu->add_child(rootless_sub);
    menu->add_child(rootful_sub);
    menu->add_child(wm_sub);
    menu->add_child(dm_sub);
    add_navigation_items(menu);

    return menu;
}

} // namespace tmoe::ui::menus
