/** DownloadTools 菜单插件 — 将 run_download_menu() 的 whiptail 菜单项
 *  展开为独立的 LambdaAction，每个包装对应的领域方法调用。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/download_tools.h"
#include "core/command_builder.hpp"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_download_menu(domain::DownloadTools* mgr) {
    auto menu = make_plugin_menu(
        _("download.menu_title"), _("download.menu_prompt"), "plugin_download");

    // 1. Aria2 管理器 → run_aria2_menu()
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_manager"), "download_aria2",
        [mgr](MenuContext&) -> bool {
            mgr->run_aria2_menu();
            return true;
        }));

    // 2. 迅雷下载器 → 内联安装逻辑
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.thunder"), "download_thunder",
        [](MenuContext&) -> bool {
            Logger::info(_("download.thunder_info"));
            Logger::info(_("download.thunder_install"));
            CommandBuilder("wget")
                .add_opt("-O", "/tmp/thunder.exe")
                .add_arg("https://down.sandai.net/thunder11/XunLeiSetup11.4.8.2110.exe")
                .add_raw("2>/dev/null || echo 'https://dl.xunlei.com/' || true")
                .execute();
            Logger::info(_("download.thunder_web") + ": https://dl.xunlei.com/");
            Logger::press_enter();
            return true;
        }));

    // 3. 视频下载器 → run_video_dl_menu()
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.video_dl"), "download_video_dl",
        [mgr](MenuContext&) -> bool {
            mgr->run_video_dl_menu();
            return true;
        }));

    // 4. 爬虫工具 → run_crawler_menu()
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler"), "download_crawler",
        [mgr](MenuContext&) -> bool {
            mgr->run_crawler_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
