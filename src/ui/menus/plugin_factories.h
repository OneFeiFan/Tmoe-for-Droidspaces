/** 所有插件工厂函数的前置声明。
 *  Manager 通过此头文件调用各领域模块的工厂函数。
 *  每个工厂函数接收对应领域模块的裸指针，返回一个注册好的 IUIMenu。 */
#pragma once
#include "ui/menu.h"
#include <memory>

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
    class GUIManager;
    class SoftwareCenter;
    class BetaFeaturesManager;
    class VirtualizationManager;
}

namespace tmoe::ui::menus {

// 浏览器（重构版——保留多个安装子项）
std::shared_ptr<IUIMenu> create_browser_menu(domain::BrowserManager* mgr);

// 简单包装模块（单 LambdaAction 包装整个 run_xxx_menu()）
std::shared_ptr<IUIMenu> create_office_menu(domain::OfficeManager* mgr);
std::shared_ptr<IUIMenu> create_download_menu(domain::DownloadTools* mgr);
std::shared_ptr<IUIMenu> create_input_method_menu(domain::InputMethodManager* mgr);
std::shared_ptr<IUIMenu> create_terminal_app_menu(domain::TerminalAppManager* mgr);
std::shared_ptr<IUIMenu> create_education_menu(domain::EducationManager* mgr);
std::shared_ptr<IUIMenu> create_dev_tools_menu(domain::DeveloperTools* mgr);
std::shared_ptr<IUIMenu> create_docker_menu(domain::DockerManager* mgr);
std::shared_ptr<IUIMenu> create_backup_menu(domain::BackupManager* mgr);
std::shared_ptr<IUIMenu> create_config_menu(domain::ConfigManager* mgr);
std::shared_ptr<IUIMenu> create_termux_menu(domain::TermuxManager* mgr);

// GUI 模块（每个入口点一个插件）
std::shared_ptr<IUIMenu> create_desktop_menu(domain::GUIManager* gui);
std::shared_ptr<IUIMenu> create_remote_desktop_menu(domain::GUIManager* gui);
std::shared_ptr<IUIMenu> create_beautify_menu(domain::GUIManager* gui);

// 跨模块导航（包装旧 run_xxx_menu()，内部回调在旧 while 循环保留）
std::shared_ptr<IUIMenu> create_software_center_menu(domain::SoftwareCenter* mgr);
std::shared_ptr<IUIMenu> create_beta_features_menu(domain::BetaFeaturesManager* mgr);
std::shared_ptr<IUIMenu> create_virtualization_menu(domain::VirtualizationManager* mgr);

} // namespace tmoe::ui::menus
