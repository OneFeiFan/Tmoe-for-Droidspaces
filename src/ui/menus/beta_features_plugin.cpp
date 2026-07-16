/** BetaFeatures 菜单插件 — 将 run_beta_menu() 的 12 个子项展平为独立 LambdaAction。
 *  跨模块回调（Virt/Education/InputMethod/TerminalApp）通过 BetaFeaturesManager 委托方法调用。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/features/beta_features.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_beta_features_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.menu_title"), _("beta.menu_prompt"), "plugin_beta_features");

    // 1: 容器/虚拟机 → 委托给 VirtualizationManager
    menu->add_child(LambdaAction::make(
        _("beta.opt1_virt"), "beta_virt",
        [mgr] { mgr->virt_delegate(); }));

    // 2: 科学与教育 → 委托给 EducationManager
    menu->add_child(LambdaAction::make(
        _("beta.opt2_science"), "beta_science",
        [mgr] { mgr->education_delegate(); }));

    // 3: 系统管理
    menu->add_child(LambdaAction::make(
        _("beta.opt3_system"), "beta_system",
        [mgr] { mgr->run_system_menu(); }));

    // 4: 商店与下载
    menu->add_child(LambdaAction::make(
        _("beta.opt4_store"), "beta_store",
        [mgr] { mgr->run_store_menu(); }));

    // 5: 视频剪辑
    menu->add_child(LambdaAction::make(
        _("beta.opt5_video"), "beta_video",
        [mgr] { mgr->run_video_menu(); }));

    // 6: 绘图
    menu->add_child(LambdaAction::make(
        _("beta.opt6_paint"), "beta_paint",
        [mgr] { mgr->run_paint_menu(); }));

    // 7: 文件管理
    menu->add_child(LambdaAction::make(
        _("beta.opt7_file"), "beta_file",
        [mgr] { mgr->run_file_menu(); }));

    // 8: 阅读器
    menu->add_child(LambdaAction::make(
        _("beta.opt8_read"), "beta_read",
        [mgr] { mgr->run_reader_menu(); }));

    // 9: 网络管理 (占位)
    menu->add_child(LambdaAction::make(
        _("beta.opt9_network"), "beta_network",
        [mgr] { mgr->run_network_menu(); }));

    // 10: 输入法 → 委托给 InputMethodManager
    menu->add_child(LambdaAction::make(
        _("beta.opt10_input"), "beta_input",
        [mgr] { mgr->input_method_delegate(); }));

    // 11: 终端 → 委托给 TerminalAppManager
    menu->add_child(LambdaAction::make(
        _("beta.opt11_terminal"), "beta_terminal",
        [mgr] { mgr->terminal_delegate(); }));

    // 12: 其他
    menu->add_child(LambdaAction::make(
        _("beta.opt12_other"), "beta_other",
        [mgr] { mgr->run_other_menu(); }));

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
