/** 浏览器菜单插件 — 演示 MenuRegistry 自注册模式。
 *
 *  此文件展示如何将一个完整的领域子菜单包装为插件：
 *  1. 继承 IUIMenu，在构造函数中组装子项（全部用 LambdaAction）
 *  2. 在文件底部用静态 AutoRegister 完成注册
 *  3. Manager 启动时从 MenuRegistry 收集所有插件组装主菜单
 */
#include "ui/plugin_helpers.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "domain/system/package_manager.h"
#include "core/system_helper.h"

namespace tmoe::ui::menus {

// ═══════════════════════════════════════════════════════
// 浏览器菜单容器
// ═══════════════════════════════════════════════════════

class BrowserPluginMenu : public SimpleMenu {
public:
    BrowserPluginMenu()
        : SimpleMenu(
            _("browser.menu_title"),
            _("browser.menu_prompt"),
            "plugin_browser")
    {
        build_children();
        add_child(std::make_shared<BackAction>());
        add_child(std::make_shared<ExitAction>());
    }

private:
    void build_children() {
        // 每个浏览器安装项都是一个 LambdaAction
        add_child(make_install_action(
            _("browser.firefox"), "inst_firefox",
            [this]() { return install_browser("firefox"); }));

        add_child(make_install_action(
            _("browser.firefox_esr"), "inst_firefox_esr",
            [this]() { return install_browser("firefox-esr"); }));

        add_child(make_install_action(
            _("browser.chromium"), "inst_chromium",
            [this]() { return install_browser("chromium"); }));

        add_child(make_install_action(
            _("browser.falkon"), "inst_falkon",
            [this]() { return install_browser("falkon"); }));

        add_child(make_install_action(
            _("browser.midori"), "inst_midori",
            [this]() { return install_browser("midori"); }));

        add_child(make_install_action(
            _("browser.epiphany"), "inst_epiphany",
            [this]() { return install_browser("epiphany"); }));

        add_child(make_install_action(
            _("browser.vivaldi"), "inst_vivaldi",
            []() {
                // Vivaldi 需要从官网下载
                return SystemHelper::download_file(
                    "https://downloads.vivaldi.com/stable/vivaldi-stable_amd64.deb",
                    "/tmp/vivaldi.deb");
            }));
    }

    bool install_browser(const std::string& pkg) {
        auto family = domain::PackageManager::detect_distro_family();
        return domain::PackageManager::install(pkg, family);
    }
};

// ═══════════════════════════════════════════════════════
// 自注册（main 之前执行）
// ═══════════════════════════════════════════════════════

static AutoRegister reg_browser(std::make_shared<BrowserPluginMenu>());

} // namespace tmoe::ui::menus
