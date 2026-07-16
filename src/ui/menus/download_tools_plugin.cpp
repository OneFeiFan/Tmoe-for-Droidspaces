/** DownloadTools 菜单插件 — aria2/视频下载器/爬虫工具展开为嵌套子菜单。
 *  不再调用旧的 whiptail 子菜单（run_aria2_menu/run_video_dl_menu/run_crawler_menu），
 *  而是构建 SimpleMenu + LambdaAction 子菜单树。 */
#include "ui/menus/download_tools_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/apps/download_tools.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"

namespace tmoe::ui::menus {

DownloadMenuPlugin::DownloadMenuPlugin(domain::DownloadTools* mgr) : mgr_(mgr) {}

// ── 顶层入口 ──────────────────────────────────────────────

std::shared_ptr<IUIMenu> DownloadMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("download.menu_title"), _("download.menu_prompt"), "plugin_download");

    // 1. Aria2 管理器 — 嵌套子菜单
    auto aria2_menu = build_aria2_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_manager"), "download_aria2",
        [aria2_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(aria2_menu);
            return true;
        }));

    // 2. 迅雷下载器 — 内联安装逻辑
    menu->add_child(LambdaAction::make(
        _("download.thunder"), "download_thunder",
        [] {
            Logger::info(_("download.thunder_info"));
            Logger::info(_("download.thunder_install"));
            CommandBuilder("wget")
                .add_opt("-O", "/tmp/thunder.exe")
                .add_arg("https://down.sandai.net/thunder11/XunLeiSetup11.4.8.2110.exe")
                .add_raw("2>/dev/null || echo 'https://dl.xunlei.com/' || true")
                .execute();
            Logger::info(_("download.thunder_web") + ": https://dl.xunlei.com/");
            Logger::press_enter();
        }));

    // 3. 视频下载器 — 嵌套子菜单
    auto video_dl_menu = build_video_dl_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.video_dl"), "download_video_dl",
        [video_dl_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(video_dl_menu);
            return true;
        }));

    // 4. 爬虫工具 — 嵌套子菜单
    auto crawler_menu = build_crawler_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler"), "download_crawler",
        [crawler_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(crawler_menu);
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── Aria2 子菜单 ──────────────────────────────────────────

std::shared_ptr<IUIMenu> DownloadMenuPlugin::build_aria2_menu() {
    auto menu = make_plugin_menu(
        _("download.aria2_menu"), "", "plugin_download_aria2");

    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_install"), "1",
        [this](MenuContext&) -> bool {
            Logger::step(_("download.aria2_installing"));
            auto family = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("aria2", family);
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_config"), "2",
        [this](MenuContext&) -> bool {
            mgr_->configure_aria2();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_start"), "3",
        [this](MenuContext&) -> bool {
            mgr_->start_aria2();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_stop"), "4",
        [this](MenuContext&) -> bool {
            mgr_->stop_aria2();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.aria2_webui"), "5",
        [this](MenuContext&) -> bool {
            mgr_->install_aria_webui();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── 视频下载器子菜单 ──────────────────────────────────────

std::shared_ptr<IUIMenu> DownloadMenuPlugin::build_video_dl_menu() {
    auto menu = make_plugin_menu(
        _("download.video_dl"), "", "plugin_download_video_dl");

    menu->add_child(std::make_shared<LambdaAction>(
        _("download.yt_dlp"), "1",
        [this](MenuContext&) -> bool {
            mgr_->install_yt_dlp();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.you_get"), "2",
        [this](MenuContext&) -> bool {
            mgr_->install_you_get();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.lux"), "3",
        [this](MenuContext&) -> bool {
            mgr_->install_lux();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.annie"), "4",
        [this](MenuContext&) -> bool {
            mgr_->install_annie();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.gallery_dl"), "5",
        [this](MenuContext&) -> bool {
            mgr_->install_gallery_dl();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── 爬虫工具子菜单 ──────────────────────────────────────

std::shared_ptr<IUIMenu> DownloadMenuPlugin::build_crawler_menu() {
    auto menu = make_plugin_menu(
        _("download.crawler"), "", "plugin_download_crawler");

    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler_httrack"), "1",
        [this](MenuContext&) -> bool {
            mgr_->install_httrack();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler_wget_mirror"), "2",
        [this](MenuContext&) -> bool {
            mgr_->show_wget_mirror_info();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler_aria2_batch"), "3",
        [this](MenuContext&) -> bool {
            mgr_->install_aria2_batch();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler_scrapy"), "4",
        [this](MenuContext&) -> bool {
            mgr_->install_scrapy();
            Logger::press_enter();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("download.crawler_curl_batch"), "5",
        [this](MenuContext&) -> bool {
            mgr_->install_curl_batch();
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
