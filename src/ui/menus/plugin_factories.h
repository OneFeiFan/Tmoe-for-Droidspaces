/** UI 插件类声明与工厂函数。
 *  Manager 通过此头文件实例化各领域模块的菜单插件。 */
#pragma once
#include "ui/menu.h"
#include "ui/menus/gui_desktop_plugin.h"
#include "ui/menus/gui_beautify_plugin.h"
#include "ui/menus/gui_remote_plugin.h"
#include "ui/menus/browser_plugin.h"
#include "ui/menus/office_plugin.h"
#include "ui/menus/terminal_app_plugin.h"
#include "ui/menus/download_tools_plugin.h"
#include "ui/menus/input_method_plugin.h"
#include "ui/menus/dev_tools_plugin.h"
#include "ui/menus/docker_plugin.h"
#include "ui/menus/backup_plugin.h"
#include "ui/menus/virtualization_plugin.h"
#include "ui/menus/education_plugin.h"
#include "ui/menus/config_plugin.h"
#include "ui/menus/termux_plugin.h"
#include "ui/menus/software_center_plugin.h"
#include "ui/menus/beta_features_plugin.h"
#include <memory>

// 其他插件类的前置声明 — 在各自 .h 中定义（Phase 3 逐步添加）
namespace tmoe::domain {
    class BrowserManager;
    class OfficeManager;
    class DownloadTools;
    class InputMethodManager;
    class TerminalAppManager;
    class EducationManager;
    class DeveloperTools;
    class DockerManager;
    class BackupManager;
    class ConfigManager;
    class TermuxManager;
    class SoftwareCenter;
    class BetaFeaturesManager;
    class VirtualizationManager;
}

namespace tmoe::ui::menus {

// ═══════════════════════════════════════════════════════════
// 工厂函数（便捷包装，内部实例化插件类）
// ═══════════════════════════════════════════════════════════

// GUI 模块
inline std::shared_ptr<IUIMenu> create_desktop_menu(domain::GUIManager* gui) {
    return DesktopMenuPlugin(gui).build();
}
inline std::shared_ptr<IUIMenu> create_beautify_menu(domain::GUIManager* gui) {
    return BeautifyMenuPlugin(gui).build();
}
inline std::shared_ptr<IUIMenu> create_remote_desktop_menu(domain::GUIManager* gui) {
    return RemoteDesktopMenuPlugin(gui).build();
}

// App/System 模块
inline std::shared_ptr<IUIMenu> create_browser_menu(domain::BrowserManager* mgr) {
    return BrowserMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_office_menu(domain::OfficeManager* mgr) {
    return OfficeMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_download_menu(domain::DownloadTools* mgr) {
    return DownloadMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_input_method_menu(domain::InputMethodManager* mgr) {
    return InputMethodMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_terminal_app_menu(domain::TerminalAppManager* mgr) {
    return TerminalAppMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_education_menu(domain::EducationManager* mgr) {
    return EducationMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_dev_tools_menu(domain::DeveloperTools* mgr) {
    return DevToolsMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_docker_menu(domain::DockerManager* mgr) {
    return DockerMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_backup_menu(domain::BackupManager* mgr) {
    return BackupMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_config_menu(domain::ConfigManager* mgr) {
    return ConfigMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_termux_menu(domain::TermuxManager* mgr) {
    return TermuxMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_software_center_menu(domain::SoftwareCenter* mgr) {
    return SoftwareCenterMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_beta_features_menu(domain::BetaFeaturesManager* mgr) {
    return BetaFeaturesMenuPlugin(mgr).build();
}
inline std::shared_ptr<IUIMenu> create_virtualization_menu(domain::VirtualizationManager* mgr) {
    return VirtualizationMenuPlugin(mgr).build();
}

} // namespace tmoe::ui::menus
