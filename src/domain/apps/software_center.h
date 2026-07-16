#ifndef SOFTWARE_CENTER_H
#define SOFTWARE_CENTER_H
#pragma once
#include "core/config.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "domain/system/package_manager.h"

namespace tmoe::domain {
    class MediaTools;
    class SoftwareCenter {
    public:
        explicit SoftwareCenter(const TmoeConfig &cfg);
        ~SoftwareCenter();

        void run_software_center_menu();

        // 跨模块回调 (Browser/Dev/Office/Download/Zsh)
        void set_browser_cb(std::function<void()> cb) { browser_cb_ = std::move(cb); }
        void set_dev_cb(std::function<void()> cb) { dev_cb_ = std::move(cb); }
        void set_office_cb(std::function<void()> cb) { office_cb_ = std::move(cb); }
        void set_download_cb(std::function<void()> cb) { download_cb_ = std::move(cb); }
        void set_zsh_cb(std::function<void()> cb) { zsh_cb_ = std::move(cb); }

        void invoke_browser_cb() { if (browser_cb_) browser_cb_(); }
        void invoke_dev_cb() { if (dev_cb_) dev_cb_(); }
        void invoke_office_cb() { if (office_cb_) office_cb_(); }
        void invoke_download_cb() { if (download_cb_) download_cb_(); }
        void invoke_zsh_cb() { if (zsh_cb_) zsh_cb_(); }

        // ── 插件可访问的子菜单 ──
        void run_sns_menu();
        void run_games_menu();
        void run_media_menu();
        void run_fileshare_menu();
        void run_pkg_gui_menu();
        void run_cleanup_menu();
        void run_electron_apps_menu();
        void run_electron_vm_menu();
        void run_debian_opt_menu();

        void electron_install_or_remove(const std::string &app_name);

        void ensure_electron_runtime();

        void remove_gui();
        void remove_browser();
        void remove_tmoe_tools();

        // ── 插件可访问的安装方法 ──
        void install_spotify();
        void install_netease_cloud_music();
        void install_with_check(const std::string &pkg, DistroFamily family);
        void install_linux_qq();
        void install_wechat();
        void install_skype();
        void install_mitalk();
        void run_image_compression_menu();

    private:

        // 内部子菜单（被公开 run_*_menu() 内部调用）
        void run_electron_music_menu();
        void run_electron_video_menu();
        void run_electron_note_menu();
        void run_electron_dev_menu();
        void run_electron_manager();
        void check_download_path();
        void install_electron_app(const std::string &app_name);
        void remove_electron_app(const std::string &app_name);
        void download_tmoe_electron_app(const std::string &app_name);
        void run_docs_menu();

        const TmoeConfig &cfg_;
        std::string apps_lnk_dir_ = "/usr/share/applications";
        std::function<void()> browser_cb_;
        std::function<void()> dev_cb_;
        std::function<void()> office_cb_;
        std::function<void()> download_cb_;
        std::function<void()> zsh_cb_;
        std::unique_ptr<MediaTools> media_tools_;
    };
} // namespace tmoe::domain
#endif
