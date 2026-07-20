/** SoftwareCenter 菜单插件 — 将 run_software_center_menu() 的 13 个菜单项
 *  分解为独立的 LambdaAction，通过 IUIMenu 框架渲染和分发。
 *  跨模块回调（Browser/Dev/Office/Download/Zsh）通过 SoftwareCenter 的
 *  公开 invoker 方法调用；子菜单直接调用 SoftwareCenter 的公共方法。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "domain/apps/software_center.h"
#include "domain/apps/media_tools.h"
#include "domain/system/package_manager.h"
#include "ui/menus/software_center_plugin.h"

namespace tmoe::ui::menus {

// ═══════════════════════════════════════════════════════════════
// build_image_compression_menu — 3 items, mirrors old MediaTools::run_image_compression_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_image_compression_menu() {
    auto mt = mgr_->media_tools();
    auto menu = make_plugin_menu(
        _("media.compress.title"), _("media.compress.menu_prompt"), "plugin_image_compress");

    menu->add_action(_("media.compress.start"), "1",
        [mt] { mt->start_compression(); });
    menu->add_action(_("media.compress.install_dep"), "2",
        [mt] { mt->install_dependencies(); });
    menu->add_action(_("media.compress.remove"), "3",
        [mt] { mt->remove_dependencies(); });

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_media_menu — 12 items, mirrors old run_media_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_media_menu() {
    auto menu = make_plugin_menu(_("swcenter.media_menu"), _("swcenter.media.menu_prompt"), "plugin_media");

    menu->add_submenu(_("swcenter.batch_compress"), "1", build_image_compression_menu());

    menu->add_action(_("swcenter.mpv"), "2", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        if (fam == domain::DistroFamily::RedHat)
            mgr_->install_with_check("kmplayer", fam);
        else
            mgr_->install_with_check("mpv", fam);
    });

    menu->add_action(_("swcenter.smplayer"), "3", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("smplayer", fam);
    });

    menu->add_action(_("swcenter.peek"), "4", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("peek", fam);
    });

    menu->add_action(_("swcenter.gimp"), "5", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("gimp", fam);
    });

    menu->add_action(_("swcenter.kolourpaint"), "6", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("kolourpaint", fam);
    });

    menu->add_action(_("swcenter.clementine"), "7", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("clementine", fam);
    });

    menu->add_action(_("swcenter.parole"), "8", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("parole", fam);
    });

    menu->add_action(_("swcenter.netease"), "9", [this] {
        mgr_->install_netease_cloud_music();
    });

    menu->add_action(_("swcenter.audacity"), "10", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("audacity", fam);
    });

    menu->add_action(_("swcenter.ardour"), "11", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        mgr_->install_with_check("ardour", fam);
    });

    menu->add_action(_("swcenter.spotify"), "12", [this] {
        mgr_->install_spotify();
    });

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_sns_menu — 10 items, mirrors old run_sns_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_sns_menu() {
    auto menu = make_plugin_menu(_("swcenter.sns_menu"), _("swcenter.sns.menu_prompt"), "plugin_sns");

    menu->add_action(_("swcenter.sns.linuxqq"), "1", [this] {
        mgr_->install_linux_qq();
    });

    menu->add_action(_("swcenter.sns.wechat"), "2", [this] {
        mgr_->install_wechat();
    });

    menu->add_action(_("swcenter.sns.thunderbird"), "3", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("thunderbird", fam);
    });

    menu->add_action(_("swcenter.sns.kmail"), "4", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("kmail", fam);
    });

    menu->add_action(_("swcenter.sns.evolution"), "5", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("evolution", fam);
    });

    menu->add_action(_("swcenter.sns.empathy"), "6", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("empathy", fam);
    });

    menu->add_action(_("swcenter.sns.pidgin"), "7", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("pidgin", fam);
    });

    menu->add_action(_("swcenter.sns.xchat"), "8", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("xchat", fam);
    });

    menu->add_action(_("swcenter.sns.skype"), "9", [this] {
        mgr_->install_skype();
    });

    menu->add_action(_("swcenter.sns.mitalk"), "10", [this] {
        mgr_->install_mitalk();
    });

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_games_menu — 8 items, mirrors old run_games_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_games_menu() {
    auto menu = make_plugin_menu(_("swcenter.games_menu"), _("swcenter.games.menu_prompt"), "plugin_games");

    menu->add_action(_("swcenter.games.kde_games"), "1", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install({"kdegames", "kde-full"}, fam);
    });

    menu->add_action(_("swcenter.games.gnome_games"), "2", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("gnome-games", fam);
    });

    menu->add_action(_("swcenter.games.steam"), "3", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("steam-launcher", fam);
    });

    menu->add_action(_("swcenter.games.cataclysm"), "4", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("cataclysm-dda", fam);
    });

    menu->add_action(_("swcenter.games.wesnoth"), "5", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("wesnoth", fam);
    });

    menu->add_action(_("swcenter.games.retroarch"), "6", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("retroarch", fam);
    });

    menu->add_action(_("swcenter.games.dolphin"), "7", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("dolphin-emu", fam);
    });

    menu->add_action(_("swcenter.games.supertuxkart"), "8", [this] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("supertuxkart", fam);
    });

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_pkg_gui_menu — 对应旧 Bash tmoe_software_package_menu
// 4 items: synaptic, gdebi, pamac, bleachbit
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_pkg_gui_menu() {
    auto menu = make_plugin_menu(_("swcenter.pkg_gui_menu"), "", "plugin_pkg_gui");
    menu->add_action(_("swcenter.synaptic"), "1", [] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("synaptic", fam);
    });
    menu->add_action(_("swcenter.gdebi"), "2", [] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("gdebi", fam);
    });
    menu->add_action(_("swcenter.pamac"), "3", [] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("pamac", fam);
    });
    menu->add_action(_("swcenter.bleachbit"), "4", [] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("bleachbit", fam);
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_fileshare_menu — 对应旧 Bash personal_netdisk
// 2 items: filebrowser (curl), nginx webdav
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_fileshare_menu() {
    auto menu = make_plugin_menu(_("swcenter.fileshare_menu"), "", "plugin_fileshare");
    menu->add_action(_("swcenter.filebrowser"), "1", [] {
        Logger::info(_("swcenter.downloading") + std::string(": FileBrowser"));
        Executor::passthrough(
            "curl -fsSL https://raw.githubusercontent.com/filebrowser/get/master/get.sh | bash || true");
    });
    menu->add_action(_("swcenter.nginx_webdav"), "2", [] {
        auto fam = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("nginx", fam);
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_cleanup_menu — 对应旧 Bash tmoe_other_options_menu
// 3 items: remove_gui, remove_browser, remove_tmoe_tools
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_cleanup_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.cleanup.menu_title"), _("swcenter.cleanup.menu_prompt"), "plugin_cleanup");
    menu->add_action(_("swcenter.cleanup_rm_gui"), "1", [this] {
        mgr_->remove_gui();
    });
    menu->add_action(_("swcenter.cleanup_rm_browser"), "2", [this] {
        mgr_->remove_browser();
    });
    menu->add_action(_("swcenter.cleanup_rm_tmoe"), "3", [this] {
        mgr_->remove_tmoe_tools();
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_debian_opt_menu — 对应旧 Bash explore_debian_opt_repo
// 9 items: ALL stubs printing not_implemented message
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_debian_opt_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.debian_opt.menu_title"), _("swcenter.debian_opt.menu_prompt"), "plugin_debian_opt");
    menu->add_action(_("swcenter.debian_opt.music"), "1", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.notes"), "2", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.videos"), "3", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.pictures"), "4", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.reader"), "5", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.games_item"), "6", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.development"), "7", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.tools"), "8", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    menu->add_action(_("swcenter.debian_opt.internet"), "9", [] {
        Logger::info(_("swcenter.debian_opt.not_implemented"));
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_vm_menu — 对应旧 Bash electron VM 子菜单
// 2 items: MacOS8, Win95 (git clone + tar extract)
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_vm_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.vm_title"), _("swcenter.electron.vm_prompt"), "plugin_electron_vm");
    menu->add_action(_("swcenter.electron.vm_macos8"), "1", [this] {
        Logger::info(_("swcenter.electron.vm_macos8_size"));
        if (!Logger::confirm(_("swcenter.electron.vm_confirm_macos8"))) {
            return;
        }
        mgr_->ensure_electron_runtime();
        Executor::passthrough(
            "cd /tmp && "
            "TEMP_FOLDER='.macintosh.js_TEMP_FOLDER'; "
            "rm -rf ${TEMP_FOLDER} 2>/dev/null; "
            "git clone --depth=1 https://gitee.com/ak2/electron_macos8.git ${TEMP_FOLDER} && "
            "cd ${TEMP_FOLDER} && "
            "cat .vm_* >vm.tar.xz && "
            "tar -PpJxvf vm.tar.xz && "
            "cd .. && rm -rf ${TEMP_FOLDER}"
        );
        Logger::ok(_("swcenter.electron.vm_macos8_done"));
    });
    menu->add_action(_("swcenter.electron.vm_win95"), "2", [this] {
        Logger::info(_("swcenter.electron.vm_win95_size"));
        if (!Logger::confirm(_("swcenter.electron.vm_confirm_win95"))) {
            return;
        }
        mgr_->ensure_electron_runtime();
        Executor::passthrough(
            "cd /tmp && "
            "TEMP_FOLDER='.windows95_TEMP_FOLDER'; "
            "rm -rf ${TEMP_FOLDER} 2>/dev/null; "
            "git clone --depth=1 https://gitee.com/ak2/electron_win95.git ${TEMP_FOLDER} && "
            "cd ${TEMP_FOLDER} && "
            "cat .vm_* >vm.tar.xz && "
            "tar -PpJxvf vm.tar.xz && "
            "cd .. && rm -rf ${TEMP_FOLDER}"
        );
        Logger::ok(_("swcenter.electron.vm_win95_done"));
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_music_menu — 4 items, mirrors old run_electron_music_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_music_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.music_menu_title"), _("swcenter.electron.music_menu_title"), "plugin_electron_music");
    menu->add_action(_("swcenter.electron.music_ncm"), "1", [this] {
        mgr_->electron_install_or_remove("electron-netease-cloud-music");
    });
    menu->add_action(_("swcenter.electron.music_yesplay"), "2", [this] {
        mgr_->electron_install_or_remove("yesplaymusic");
    });
    menu->add_action(_("swcenter.electron.music_listen1"), "3", [this] {
        mgr_->electron_install_or_remove("listen1");
    });
    menu->add_action(_("swcenter.electron.music_petal"), "4", [this] {
        mgr_->electron_install_or_remove("petal");
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_video_menu — 4 items, mirrors old run_electron_video_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_video_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.video_menu_title"), _("swcenter.electron.video_menu_prompt"), "plugin_electron_video");
    menu->add_action(_("swcenter.electron.video_bilibili"), "1", [this] {
        mgr_->electron_install_or_remove("bilibili");
    });
    menu->add_action(_("swcenter.electron.video_zyplayer"), "2", [this] {
        mgr_->electron_install_or_remove("zy-player");
    });
    menu->add_action(_("swcenter.electron.video_tencent"), "3", [this] {
        mgr_->electron_install_or_remove("tenvideo_universal");
    });
    menu->add_action(_("swcenter.electron.video_lossless_cut"), "4", [this] {
        auto family = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install("ffmpeg", family);
        mgr_->electron_install_or_remove("lossless-cut");
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_note_menu — 4 items, mirrors old run_electron_note_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_note_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.note_menu_title"), _("swcenter.electron.note_menu_prompt"), "plugin_electron_note");
    menu->add_action(_("swcenter.electron.note_obsidian"), "1", [this] {
        mgr_->electron_install_or_remove("obsidian");
    });
    menu->add_action(_("swcenter.electron.note_gridea"), "2", [this] {
        mgr_->electron_install_or_remove("gridea");
    });
    menu->add_action(_("swcenter.electron.note_drawio"), "3", [this] {
        auto family = domain::PackageManager::detect_distro_family();
        domain::PackageManager::install({"libindicator3-7", "libappindicator3-1"}, family);
        mgr_->electron_install_or_remove("draw.io");
    });
    menu->add_action(_("swcenter.electron.note_simplenote"), "4", [this] {
        mgr_->electron_install_or_remove("simplenote");
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_dev_menu — 1 item, mirrors old run_electron_dev_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_dev_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.dev_title"), _("swcenter.electron.dev_prompt"), "plugin_electron_dev");
    menu->add_action(_("swcenter.electron.dev_netron"), "1", [this] {
        mgr_->electron_install_or_remove("netron");
    });
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_manager_menu — 3 items, mirrors old run_electron_manager()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_manager_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.title_manager"), _("swcenter.electron.mgr_prompt"), "plugin_electron_manager");
    menu->add_action(_("swcenter.electron.mgr_install"), "1", [this] {
        mgr_->ensure_electron_runtime();
    });
    menu->add_action(_("swcenter.electron.mgr_remove_stable"), "2", [this] {
        Logger::warn(_("swcenter.electron.mgr_remove_warn"));
        Logger::warn(_("swcenter.electron.runtime_rm_path_warn"));
        if (Logger::confirm(_("swcenter.electron.mgr_confirm_remove"))) {
            CommandBuilder("sudo").add_arg("rm").add_flag("-rf")
                .add_arg("/opt/electron")
                .add_arg("/usr/bin/electron")
                .add_raw("2>/dev/null").execute();
            Logger::ok(_("swcenter.electron.mgr_removed"));
        }
    });
    menu->add_action(_("swcenter.electron.mgr_remove_v8"), "3", [this] {
        Logger::warn(_("swcenter.electron.mgr_remove_v8_warn"));
        Logger::warn(_("swcenter.electron.runtime_rm_v8_path_warn"));
        if (Logger::confirm(_("swcenter.electron.mgr_confirm_remove_v8"))) {
            CommandBuilder("sudo").add_arg("rm").add_flag("-rf").add_arg("/opt/electron-v8").add_raw("2>/dev/null").execute();
            Logger::ok(_("swcenter.electron.mgr_removed_v8"));
        }
    });
    add_sandwich_nav(menu);
    return menu;
}

std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("swcenter.menu_title"), _("swcenter.menu_prompt"), "plugin_software_center");

    // 1: Browser → cross-module callback
    menu->add_action(_("swcenter.browser"), "1",
        [this] { mgr_->invoke_browser_cb(); });

    // 2: Dev → cross-module callback
    menu->add_action(_("swcenter.dev"), "2",
        [this] { mgr_->invoke_dev_cb(); });

    // 3: Electron → nested sub-menu
    auto electron_apps_menu = make_plugin_menu(
        _("swcenter.electron"), _("swcenter.electron.menu_prompt"), "plugin_electron_apps");
    electron_apps_menu->add_child(build_electron_music_menu());
    electron_apps_menu->add_child(build_electron_video_menu());
    electron_apps_menu->add_child(build_electron_note_menu());
    electron_apps_menu->add_child(build_electron_vm_menu());
    electron_apps_menu->add_child(build_electron_dev_menu());
    electron_apps_menu->add_child(build_electron_manager_menu());
    add_navigation_items(electron_apps_menu);
    menu->add_submenu(_("swcenter.electron"), "3", electron_apps_menu);

    // 4: Media → sub-menu via IUIMenu framework
    menu->add_submenu(_("swcenter.media"), "4", build_media_menu());

    // 5: SNS → sub-menu via IUIMenu framework
    menu->add_submenu(_("swcenter.sns"), "5", build_sns_menu());

    // 6: Games → sub-menu via IUIMenu framework
    menu->add_submenu(_("swcenter.games"), "6", build_games_menu());

    // 7: Docs → cross-module callback (office)
    menu->add_action(_("swcenter.docs"), "7",
        [this] { mgr_->invoke_office_cb(); });

    // 8: Debian Opt Repo → sub-menu
    menu->add_submenu(_("swcenter.debian_opt"), "8", build_debian_opt_menu());

    // 9: Download → cross-module callback
    menu->add_action(_("swcenter.download"), "9",
        [this] { mgr_->invoke_download_cb(); });

    // 10: Pkg GUI → sub-menu
    menu->add_submenu(_("swcenter.pkg_gui"), "10", build_pkg_gui_menu());

    // 11: Zsh → cross-module callback
    menu->add_action(_("swcenter.zsh"), "11",
        [this] { mgr_->invoke_zsh_cb(); });

    // 12: File Shared → sub-menu
    menu->add_submenu(_("swcenter.fileshare"), "12", build_fileshare_menu());

    // 13: Cleanup → sub-menu
    menu->add_submenu(_("swcenter.cleanup"), "13", build_cleanup_menu());

    add_navigation_items(menu);
    return menu;
}

} // namespace tmoe::ui::menus
