/** Termux 菜单插件 — 将 run_termux_menu() 中每一项映射为独立的 LambdaAction。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/termux.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_termux_menu(domain::TermuxManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.termux"), _("termux.menu_prompt"), "plugin_termux");

    // 1. GUI 子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.gui"), "termux_gui",
        [mgr](MenuContext&) -> bool {
            mgr->run_termux_gui_menu();
            return true;
        }));

    // 2. 备份
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.backup"), "termux_backup",
        [mgr](MenuContext&) -> bool {
            mgr->backup_termux();
            return true;
        }));

    // 3. 还原
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.restore"), "termux_restore",
        [mgr](MenuContext&) -> bool {
            mgr->restore_termux();
            return true;
        }));

    // 4. 终端美化
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.beautify"), "termux_beautify",
        [mgr](MenuContext&) -> bool {
            mgr->beautify_terminal();
            return true;
        }));

    // 5. 镜像源切换
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.mirror"), "termux_mirror",
        [mgr](MenuContext&) -> bool {
            mgr->switch_pkg_mirror();
            return true;
        }));

    // 6. 磁盘空间占用
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.disk_usage"), "termux_disk",
        [mgr](MenuContext&) -> bool {
            mgr->check_disk_usage();
            return true;
        }));

    // 7. 环境初始化
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.env_init"), "termux_env_init",
        [mgr](MenuContext&) -> bool {
            mgr->check_and_init_environment();
            return true;
        }));

    // 8. Android 12+ Signal 9 修复
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.signal9"), "termux_signal9",
        [mgr](MenuContext&) -> bool {
            mgr->fix_android_12_signal_9();
            return true;
        }));

    // 9. 共享存储设置
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.storage"), "termux_storage",
        [mgr](MenuContext&) -> bool {
            mgr->setup_storage();
            return true;
        }));

    // A. PulseAudio 配置
    // 旧代码中为内联 whiptail 子菜单 (TCP / LAN 切换 / 空闲超时)；
    // 新框架下直接调用主配置方法, cfg_ 为私有成员无法在插件中复现 TUI 子菜单。
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.pulseaudio"), "termux_pulseaudio",
        [mgr](MenuContext&) -> bool {
            mgr->configure_pulseaudio_tcp();
            return true;
        }));

    // B. 自更新
    menu->add_child(std::make_shared<LambdaAction>(
        _("termux.self_update"), "termux_self_update",
        [mgr](MenuContext&) -> bool {
            mgr->self_update();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
