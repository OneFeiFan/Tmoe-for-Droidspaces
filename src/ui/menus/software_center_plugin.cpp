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

    menu->add_child(std::make_shared<LambdaAction>(
        _("media.compress.start"), "1",
        [mt](MenuContext& ctx) -> bool {
            mt->start_compression();
            return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("media.compress.install_dep"), "2",
        [mt](MenuContext&) -> bool {
            mt->install_dependencies();
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("media.compress.remove"), "3",
        [mt](MenuContext&) -> bool {
            mt->remove_dependencies();
            Logger::press_enter(); return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_media_menu — 12 items, mirrors old run_media_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_media_menu() {
    auto menu = make_plugin_menu(_("swcenter.media_menu"), _("swcenter.media.menu_prompt"), "plugin_media");

    auto img_menu = build_image_compression_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.batch_compress"), "1",
        [img_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(img_menu);
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.mpv"), "2",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            if (fam == domain::DistroFamily::RedHat)
                mgr_->install_with_check("kmplayer", fam);
            else
                mgr_->install_with_check("mpv", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.smplayer"), "3",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("smplayer", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.peek"), "4",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("peek", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.gimp"), "5",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("gimp", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.kolourpaint"), "6",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("kolourpaint", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.clementine"), "7",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("clementine", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.parole"), "8",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("parole", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.netease"), "9",
        [this](MenuContext&) -> bool {
            mgr_->install_netease_cloud_music();
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.audacity"), "10",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("audacity", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.ardour"), "11",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            mgr_->install_with_check("ardour", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.spotify"), "12",
        [this](MenuContext&) -> bool {
            mgr_->install_spotify();
            Logger::press_enter(); return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_sns_menu — 10 items, mirrors old run_sns_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_sns_menu() {
    auto menu = make_plugin_menu(_("swcenter.sns_menu"), _("swcenter.sns.menu_prompt"), "plugin_sns");

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.linuxqq"), "1",
        [this](MenuContext&) -> bool {
            mgr_->install_linux_qq();
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.wechat"), "2",
        [this](MenuContext&) -> bool {
            mgr_->install_wechat();
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.thunderbird"), "3",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("thunderbird", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.kmail"), "4",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("kmail", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.evolution"), "5",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("evolution", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.empathy"), "6",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("empathy", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.pidgin"), "7",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("pidgin", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.xchat"), "8",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("xchat", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.skype"), "9",
        [this](MenuContext&) -> bool {
            mgr_->install_skype();
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns.mitalk"), "10",
        [this](MenuContext&) -> bool {
            mgr_->install_mitalk();
            Logger::press_enter(); return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_games_menu — 8 items, mirrors old run_games_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_games_menu() {
    auto menu = make_plugin_menu(_("swcenter.games_menu"), _("swcenter.games.menu_prompt"), "plugin_games");

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.kde_games"), "1",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"kdegames", "kde-full"}, fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.gnome_games"), "2",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("gnome-games", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.steam"), "3",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("steam-launcher", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.cataclysm"), "4",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("cataclysm-dda", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.wesnoth"), "5",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("wesnoth", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.retroarch"), "6",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("retroarch", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.dolphin"), "7",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("dolphin-emu", fam);
            Logger::press_enter(); return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games.supertuxkart"), "8",
        [this](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("supertuxkart", fam);
            Logger::press_enter(); return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_pkg_gui_menu — 对应旧 Bash tmoe_software_package_menu
// 4 items: synaptic, gdebi, pamac, bleachbit
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_pkg_gui_menu() {
    auto menu = make_plugin_menu(_("swcenter.pkg_gui_menu"), "", "plugin_pkg_gui");
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.synaptic"), "1",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("synaptic", fam);
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.gdebi"), "2",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("gdebi", fam);
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.pamac"), "3",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("pamac", fam);
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.bleachbit"), "4",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("bleachbit", fam);
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_fileshare_menu — 对应旧 Bash personal_netdisk
// 2 items: filebrowser (curl), nginx webdav
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_fileshare_menu() {
    auto menu = make_plugin_menu(_("swcenter.fileshare_menu"), "", "plugin_fileshare");
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.filebrowser"), "1",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.downloading") + std::string(": FileBrowser"));
            Executor::passthrough(
                "curl -fsSL https://raw.githubusercontent.com/filebrowser/get/master/get.sh | bash || true");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.nginx_webdav"), "2",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("nginx", fam);
            Logger::press_enter(); return true;
        }));
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
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.cleanup_rm_gui"), "1",
        [this](MenuContext&) -> bool {
            mgr_->remove_gui();
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.cleanup_rm_browser"), "2",
        [this](MenuContext&) -> bool {
            mgr_->remove_browser();
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.cleanup_rm_tmoe"), "3",
        [this](MenuContext&) -> bool {
            mgr_->remove_tmoe_tools();
            Logger::press_enter(); return true;
        }));
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
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.music"), "1",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.notes"), "2",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.videos"), "3",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.pictures"), "4",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.reader"), "5",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.games_item"), "6",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.development"), "7",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.tools"), "8",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.debian_opt.internet"), "9",
        [](MenuContext&) -> bool {
            Logger::info(_("swcenter.debian_opt.not_implemented"));
            Logger::press_enter(); return true;
        }));
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
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.electron.vm_macos8"), "1",
        [this](MenuContext&) -> bool {
            Logger::info(_("swcenter.electron.vm_macos8_size"));
            if (!Logger::confirm(_("swcenter.electron.vm_confirm_macos8"))) {
                Logger::press_enter(); return true;
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
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(_("swcenter.electron.vm_win95"), "2",
        [this](MenuContext&) -> bool {
            Logger::info(_("swcenter.electron.vm_win95_size"));
            if (!Logger::confirm(_("swcenter.electron.vm_confirm_win95"))) {
                Logger::press_enter(); return true;
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
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_music_menu — 4 items, mirrors old run_electron_music_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_music_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.music_menu_title"), _("swcenter.electron.music_menu_title"), "plugin_electron_music");
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.music_ncm"), "1",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("electron-netease-cloud-music");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.music_yesplay"), "2",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("yesplaymusic");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.music_listen1"), "3",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("listen1");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.music_petal"), "4",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("petal");
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_video_menu — 4 items, mirrors old run_electron_video_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_video_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.video_menu_title"), _("swcenter.electron.video_menu_prompt"), "plugin_electron_video");
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.video_bilibili"), "1",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("bilibili");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.video_zyplayer"), "2",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("zy-player");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.video_tencent"), "3",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("tenvideo_universal");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.video_lossless_cut"), "4",
        [this](MenuContext&) -> bool {
            auto family = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("ffmpeg", family);
            mgr_->electron_install_or_remove("lossless-cut");
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_note_menu — 4 items, mirrors old run_electron_note_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_note_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.note_menu_title"), _("swcenter.electron.note_menu_prompt"), "plugin_electron_note");
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.note_obsidian"), "1",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("obsidian");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.note_gridea"), "2",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("gridea");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.note_drawio"), "3",
        [this](MenuContext&) -> bool {
            auto family = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"libindicator3-7", "libappindicator3-1"}, family);
            mgr_->electron_install_or_remove("draw.io");
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.note_simplenote"), "4",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("simplenote");
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_dev_menu — 1 item, mirrors old run_electron_dev_menu()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_dev_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.dev_title"), _("swcenter.electron.dev_prompt"), "plugin_electron_dev");
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.dev_netron"), "1",
        [this](MenuContext&) -> bool {
            mgr_->electron_install_or_remove("netron");
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

// ═══════════════════════════════════════════════════════════════
// build_electron_manager_menu — 3 items, mirrors old run_electron_manager()
// ═══════════════════════════════════════════════════════════════
std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build_electron_manager_menu() {
    auto menu = make_plugin_menu(
        _("swcenter.electron.title_manager"), _("swcenter.electron.mgr_prompt"), "plugin_electron_manager");
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.mgr_install"), "1",
        [this](MenuContext&) -> bool {
            mgr_->ensure_electron_runtime();
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.mgr_remove_stable"), "2",
        [this](MenuContext&) -> bool {
            Logger::warn(_("swcenter.electron.mgr_remove_warn"));
            Logger::warn(_("swcenter.electron.runtime_rm_path_warn"));
            if (Logger::confirm(_("swcenter.electron.mgr_confirm_remove"))) {
                CommandBuilder("sudo").add_arg("rm").add_flag("-rf")
                    .add_arg("/opt/electron")
                    .add_arg("/usr/bin/electron")
                    .add_raw("2>/dev/null").execute();
                Logger::ok(_("swcenter.electron.mgr_removed"));
            }
            Logger::press_enter(); return true;
        }));
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.electron.mgr_remove_v8"), "3",
        [this](MenuContext&) -> bool {
            Logger::warn(_("swcenter.electron.mgr_remove_v8_warn"));
            Logger::warn(_("swcenter.electron.runtime_rm_v8_path_warn"));
            if (Logger::confirm(_("swcenter.electron.mgr_confirm_remove_v8"))) {
                CommandBuilder("sudo").add_arg("rm").add_flag("-rf").add_arg("/opt/electron-v8").add_raw("2>/dev/null").execute();
                Logger::ok(_("swcenter.electron.mgr_removed_v8"));
            }
            Logger::press_enter(); return true;
        }));
    add_sandwich_nav(menu);
    return menu;
}

std::shared_ptr<IUIMenu> SoftwareCenterMenuPlugin::build() {
    auto menu = make_plugin_menu(
        _("swcenter.menu_title"), _("swcenter.menu_prompt"), "plugin_software_center");

    // 1: Browser → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.browser"), "1",
        [this](MenuContext&) -> bool {
            mgr_->invoke_browser_cb();
            return true;
        }));

    // 2: Dev → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.dev"), "2",
        [this](MenuContext&) -> bool {
            mgr_->invoke_dev_cb();
            return true;
        }));

    // 3: Electron → nested sub-menu (new IUIMenu)
    {
        auto electron_apps_menu = make_plugin_menu(
            _("swcenter.electron"), _("swcenter.electron.menu_prompt"), "plugin_electron_apps");
        electron_apps_menu->add_child(build_electron_music_menu());
        electron_apps_menu->add_child(build_electron_video_menu());
        electron_apps_menu->add_child(build_electron_note_menu());
        electron_apps_menu->add_child(build_electron_vm_menu());
        electron_apps_menu->add_child(build_electron_dev_menu());
        electron_apps_menu->add_child(build_electron_manager_menu());
        add_navigation_items(electron_apps_menu);
        menu->add_child(std::make_shared<LambdaAction>(
            _("swcenter.electron"), "3",
            [electron_apps_menu](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(electron_apps_menu);
                return true;
            }));
    }

    // 4: Media → sub-menu via IUIMenu framework
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.media"), "4",
        [this](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_media_menu());
            return true;
        }));

    // 5: SNS → sub-menu via IUIMenu framework
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.sns"), "5",
        [this](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_sns_menu());
            return true;
        }));

    // 6: Games → sub-menu via IUIMenu framework
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.games"), "6",
        [this](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_games_menu());
            return true;
        }));

    // 7: Docs → cross-module callback (office)
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.docs"), "7",
        [this](MenuContext&) -> bool {
            mgr_->invoke_office_cb();
            return true;
        }));

    // 8: Debian Opt Repo → sub-menu (new IUIMenu)
    auto deb_opt_menu = build_debian_opt_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.debian_opt"), "8",
        [deb_opt_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(deb_opt_menu);
            return true;
        }));

    // 9: Download → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.download"), "9",
        [this](MenuContext&) -> bool {
            mgr_->invoke_download_cb();
            return true;
        }));

    // 10: Pkg GUI → sub-menu (new IUIMenu)
    auto pkg_gui_menu = build_pkg_gui_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.pkg_gui"), "10",
        [pkg_gui_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(pkg_gui_menu);
            return true;
        }));

    // 11: Zsh → cross-module callback
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.zsh"), "11",
        [this](MenuContext&) -> bool {
            mgr_->invoke_zsh_cb();
            return true;
        }));

    // 12: File Shared → sub-menu (new IUIMenu)
    auto fileshare_menu = build_fileshare_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.fileshare"), "12",
        [fileshare_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(fileshare_menu);
            return true;
        }));

    // 13: Cleanup → sub-menu (new IUIMenu)
    auto cleanup_menu = build_cleanup_menu();
    menu->add_child(std::make_shared<LambdaAction>(
        _("swcenter.cleanup"), "13",
        [cleanup_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(cleanup_menu);
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

SoftwareCenterMenuPlugin::SoftwareCenterMenuPlugin(domain::SoftwareCenter* mgr) : mgr_(mgr) {}

} // namespace tmoe::ui::menus