/** GUI 桌面安装菜单插件 — DesktopMenuPlugin 类。
 *
 *  菜单结构:
 *  ┌─ 顶层 SimpleMenu (add_navigation_items)
 *  │   ├─ 免 Root 桌面 (IUIMenu) → 动态 DE 列表 + sandwich nav
 *  │   ├─ 需 Root 桌面 (IUIMenu) → 动态 DE 列表 + sandwich nav
 *  │   ├─ 窗口管理器  (IUIMenu) → 动态 WM 列表 + sandwich nav
 *  │   └─ 显示管理器  (IUIMenu) → 固定 DM 列表 + sandwich nav
 */
#include "ui/menus/gui_desktop_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/gui/gui.hpp"
#include "domain/system/package_manager.h"

namespace tmoe::ui::menus {

DesktopMenuPlugin::DesktopMenuPlugin(domain::GUIManager* gui) : gui_(gui) {}

std::shared_ptr<IUIMenu> DesktopMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("gui.de_install_title"), _("menu.tui.title"), "plugin_gui_desktop");

    menu->add_child(build_rootless_menu());
    menu->add_child(build_rootful_menu());
    menu->add_child(build_wm_menu());
    menu->add_child(build_dm_menu());
    add_navigation_items(menu);

    return menu;
}

// ═══════════════════════════════════════════════════════════
// 子菜单: 免 Root 桌面
// ═══════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> DesktopMenuPlugin::build_rootless_menu() {
    auto sub = make_plugin_menu(
        _("gui.de_rootless"), _("menu.tui.title"), "plugin_gui_de_rootless");
    for (const auto& d : gui_->desktop_manager().desktop_registry()) {
        if (!d.requires_root && !d.is_window_manager) {
            sub->add_child(std::make_shared<LambdaAction>(
                d.name + " (" + d.compat_notes + ")",
                "de_install_" + d.id,
                [gui = gui_, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_desktop(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(sub);
    return sub;
}

// ═══════════════════════════════════════════════════════════
// 子菜单: 需 Root 桌面
// ═══════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> DesktopMenuPlugin::build_rootful_menu() {
    auto sub = make_plugin_menu(
        _("gui.de_rootful"), _("menu.tui.title"), "plugin_gui_de_rootful");
    for (const auto& d : gui_->desktop_manager().desktop_registry()) {
        if (d.requires_root && !d.is_window_manager) {
            sub->add_child(std::make_shared<LambdaAction>(
                d.name + " (" + d.compat_notes + ")",
                "de_install_" + d.id,
                [gui = gui_, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_desktop(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(sub);
    return sub;
}

// ═══════════════════════════════════════════════════════════
// 子菜单: 窗口管理器
// ═══════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> DesktopMenuPlugin::build_wm_menu() {
    auto sub = make_plugin_menu(
        _("gui.wm_title"), _("gui.wm_prompt"), "plugin_gui_de_wm");
    for (const auto& d : gui_->desktop_manager().desktop_registry()) {
        if (d.is_window_manager) {
            auto wm_label = d.name
                + (d.compat_notes.empty() ? "" : " (" + d.compat_notes + ")");
            sub->add_child(std::make_shared<LambdaAction>(
                wm_label,
                "wm_install_" + d.id,
                [gui = gui_, id = d.id](MenuContext&) -> bool {
                    gui->desktop_manager().install_window_manager(id);
                    gui->desktop_manager().after_desktop_install_hint();
                    Logger::press_enter();
                    return true;
                }));
        }
    }
    add_sandwich_nav(sub);
    return sub;
}

// ═══════════════════════════════════════════════════════════
// 子菜单: 显示管理器
// ═══════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> DesktopMenuPlugin::build_dm_menu() {
    auto sub = make_plugin_menu(
        _("gui.dm_title"), _("gui.dm_prompt"), "plugin_gui_de_dm");

    sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.lightdm"), "dm_install_lightdm",
        [gui = gui_](MenuContext&) -> bool {
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
            gui->desktop_manager().tmoe_display_manager_systemctl("lightdm", "lightdm");
            Logger::press_enter();
            return true;
        }));

    sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.sddm"), "dm_install_sddm",
        [gui = gui_](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"sddm", "sddm-theme-breeze"}, fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("sddm", "sddm");
            Logger::press_enter();
            return true;
        }));

    sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.gdm"), "dm_install_gdm",
        [gui = gui_](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"gdm", "gdm3"}, fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("gdm3", "gdm");
            Logger::press_enter();
            return true;
        }));

    sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.slim"), "dm_install_slim",
        [gui = gui_](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("slim", fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("slim", "slim");
            Logger::press_enter();
            return true;
        }));

    sub->add_child(std::make_shared<LambdaAction>(
        _("gui.dm.lxdm"), "dm_install_lxdm",
        [gui = gui_](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("lxdm", fam);
            gui->desktop_manager().tmoe_display_manager_systemctl("lxdm", "lxdm");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(sub);
    return sub;
}

} // namespace tmoe::ui::menus
