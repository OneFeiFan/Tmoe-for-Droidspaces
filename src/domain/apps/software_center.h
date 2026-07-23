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

        MediaTools *media_tools() { return media_tools_.get(); }

        // ── Debian Opt 仓库 (对应 Bash explore_debian_opt_repo, 820行) ──
        void ensure_debian_opt_repo();

        void debian_opt_install_or_remove(const std::string &name);

    private:
        void remove_electron_app(const std::string &app_name);

        void download_tmoe_electron_app(const std::string &app_name);

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
