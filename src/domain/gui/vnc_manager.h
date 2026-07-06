#pragma once

#include "core/config.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "core/system_helper.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"

#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <map>


namespace fs = std::filesystem;

namespace tmoe::domain {
    // ---------- VncConfig 结构体 ----------
    struct VncConfig {
        // 服务端类型
        std::string server = "tiger"; // "tiger" | "tight"
        std::string server_bin; // "tigervnc" | "tightvnc"

        // 连接参数
        int display = 2;
        int rfb_port = 5902;
        int resolution_w = 1440;
        int resolution_h = 720;
        int pixel_depth = 24;
        int zlib_level = 0;
        bool compare_fb = true;

        // 安全
        std::string password;
        fs::path passwd_file;
        bool always_shared = true;
        bool localhost_only = false;

        // 桌面信息
        std::string desktop_name;

        // PulseAudio / WSL
        std::string pulse_server;
        int pulse_port = 4713;
        std::string windows_ip;

        // 路径
        fs::path vnc_home_dir;
        fs::path xstartup_file;
        fs::path xsession_file;
        fs::path vnc_pid_file;
        fs::path x_pid_file;

        // 依赖包名
        std::string dep_viewer;
        std::string dep_server;
        std::string dep_extra;

        void init_defaults();

        void update_port();
    };

    // ---------- VncManager 类 ----------
    class VncManager {
    public:
        explicit VncManager(const TmoeConfig &cfg);

        // 禁止拷贝
        VncManager(const VncManager &) = delete;

        VncManager &operator=(const VncManager &) = delete;

        // ---------- 公共接口 ----------
        // 安装
        bool install_vnc_server();

        bool check_xvnc_command();

        bool choose_vnc_server();

        // 密码 / 默认配置
        bool configure_vnc_password(std::string_view password = "");

        bool configure_vnc_defaults();

        bool configure_xstartup(std::string_view desktop);

        // 启动 / 停止
        bool start_vnc(int display = -1, int width = 0, int height = 0);

        bool stop_vnc(int display = -1);

        bool start_x11vnc(int display = -1);

        bool stop_x11vnc();

        bool kill_all_vnc();

        // 状态查询
        [[nodiscard]] bool is_vnc_running(int display = -1) const;

        [[nodiscard]] int detect_available_display() const;

        [[nodiscard]] std::string get_vnc_connection_uri() const;

        [[nodiscard]] std::string get_local_ip_addresses() const;

        // 环境检测
        void detect_wsl_environment();

        // D-Bus / 脚本部署
        bool launch_dbus_daemon();

        bool fix_vnc_dbus();

        bool deploy_startup_scripts();

        // 配置脚本（被 first_configure_vnc 调用）
        void configure_startvnc();

        void configure_startxsdl();

        // 其他辅助
        void check_vnc_resolution();

        void modify_vnc_conf();

        // 主题/配置辅助
        void modify_to_xfwm4_breeze_theme();

        // x11vnc 配置修改（TUI子菜单使用）
        void modify_x11vnc_pulse_server();

        void modify_x11vnc_resolution();

        void modify_x11vnc_port();

        // D-Bus 停止/状态
        bool stop_dbus_daemon();

        void show_dbus_status() const;

        // 安装辅助（被 GUIManager 调用）
        void check_the_which_command();

        bool is_arm_container() const;

        void auto_select_keyboard_layout();

        void fix_mlocate();

        // 配置访问
        [[nodiscard]] const VncConfig &config() const { return vnc_config_; }
        VncConfig &config() { return vnc_config_; }

    private:
        const TmoeConfig &cfg_;
        VncConfig vnc_config_;

        // ---------- 内部安装辅助 ----------
        void tiger_vnc_variable();

        void tight_vnc_variable();

        void debian_remove_vnc_server();

        void debian_install_vnc_server();

        std::string grep_tiger_vnc_deb_file(const std::string &repo,
                                            const std::string &grep1,
                                            const std::string &grep2);

        void ubuntu_install_tiger_vnc_server();

        void case_debian_distro_and_install_vnc();

        void which_vnc_server_do_you_prefer();

        void create_the_which_script();

        // ---------- 命令构建 ----------
        [[nodiscard]] std::string build_vnc_start_command(int display, int width, int height) const;

        [[nodiscard]] std::string build_xvfb_command(int display, int width, int height) const;

        [[nodiscard]] std::string build_x11vnc_command(int display) const;

        // ---------- 内容生成 ----------
        [[nodiscard]] std::string generate_xsession_content(std::string_view desktop) const;

        [[nodiscard]] std::string generate_xstartup_content() const;

        // ---------- PID 管理 ----------
        void write_vnc_pid_file(int display) const;

        void remove_vnc_pid_file(int display) const;

        // ---------- 环境检测 ----------
        bool detect_android_resolution(int &width, int &height) const;

        // ---------- 辅助工具 ----------
        [[nodiscard]] bool ensure_vnc_home_dir() const;
        [[nodiscard]] bool write_file_content(const fs::path &path, std::string_view content) const;
    };
} // namespace tmoe::domain
