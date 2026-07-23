#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "core/config.h"
#include "vnc_manager.h"
#include "desktop_manager.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    class RemoteDesktopManager {
    public:
        RemoteDesktopManager(const TmoeConfig &cfg, VncManager &vnc_manager, DesktopManager &desktop_manager);

        // ---------- noVNC ----------
        bool install_novnc();

        bool configure_novnc();

        bool start_novnc(int port = -1);

        bool stop_novnc();

        bool remove_novnc();

        std::string get_novnc_url() const;

        // ---------- XRDP ----------
        bool install_xrdp();

        bool configure_xrdp_session(std::string_view desktop);

        bool start_xrdp();

        bool stop_xrdp();

        bool restart_xrdp();

        bool set_xrdp_port(int port);

        bool remove_xrdp();

        // ---------- x11vnc 配置 UI / 包装 ----------
        void x11vnc_warning() const;

        void x11vnc_onekey();

        void remove_x11vnc_ext();

        void x11vnc_doc() const;

        void x11vnc_process_readme() const;

        void do_you_want_to_configure_novnc();

        // ---------- 远程桌面环境配置 ----------
        void configure_remote_desktop_environment(std::string_view context);

        void configure_xrdp_desktop();

        void configure_x11vnc_remote_desktop_session();

        void configure_xrdp_remote_desktop_session(const std::string &session_cmd);

        // ---------- XRDP 辅助 ----------
        void xrdp_onekey();

        void check_xrdp_status() const;

        void xrdp_pulse_server();

        void xrdp_systemd() const;

        void xrdp_reset();

        void xrdp_restart();

        void xrdp_port();

        // ---------- TUI 菜单 ----------
        void run_x11vnc_config_menu();

        void run_novnc_config_menu();

        void run_xrdp_menu();

        // ---------- 自动安装标志 ----------
        void set_auto_install_mode(bool v) { auto_install_mode_ = v; }

    private:
        /// 返回 sudo 前缀。proot/Termux/已是 root 则不需要 sudo
        std::string sudo_cmd() const;

        const TmoeConfig &cfg_;
        VncManager &vnc_manager_;
        DesktopManager &desktop_manager_;
        int novnc_port_ = 36080;
        bool auto_install_mode_ = false;

        bool install_packages(const std::vector<std::string> &packages) const;

        std::string generate_polkit_colord_conf() const;

        std::string generate_polkit_colord_pkla() const;
    };
} // namespace tmoe::domain
