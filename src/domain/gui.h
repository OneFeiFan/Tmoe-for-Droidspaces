#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/command_builder.hpp"
#include "gui_config/templates.h"
#include "gui_config/registries.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {
    /** VNC 服务端运行时配置。
     *  对应 Bash 变量: VNC_SERVER, VNC_DISPLAY, RFB_PORT, VNC_RESOLUTION, PIXEL_DEPTH 等。
     */
    struct VncConfig {
        // ── 服务端类型 ──
        std::string server = "tiger"; // tiger | tight | x11vnc
        std::string server_bin; // tigervnc | tightvnc

        // ── 连接参数 ──
        int display = 2; // VNC display 编号 (:2)
        int rfb_port = 5902; // RFB 端口 (5900 + display)
        int resolution_w = 1440; // 分辨率宽度
        int resolution_h = 720; // 分辨率高度
        int pixel_depth = 24; // 像素深度 (16/24)
        int zlib_level = 0; // Zlib 压缩级别 (0-9)
        bool compare_fb = true; // TigerVNC CompareFB (仅传输变化的帧)

        // ── 安全 ──
        std::string password; // VNC 密码 (6-8 位)
        fs::path passwd_file; // 密码文件路径 (~/.vnc/passwd)
        bool always_shared = true; // 总是共享连接
        bool localhost_only = false; // 仅本地连接

        // ── 桌面信息 ──
        std::string desktop_name; // VNC 桌面名称 (从 /etc/os-release 读取)

        // ── PulseAudio ──
        std::string pulse_server; // PULSE_SERVER 地址
        int pulse_port = 4713;

        // ── WSL ──
        std::string vcxsrv_display_port = "37985";
        std::string windows_ip; // WSL2 网关 IP

        // ── 路径 ──
        fs::path vnc_home_dir; // ~/.vnc/
        fs::path xstartup_file; // ~/.vnc/xstartup
        fs::path xsession_file; // /etc/X11/xinit/Xsession
        fs::path tigervnc_config; // /etc/tigervnc/vncserver-config-tmoe
        fs::path vnc_pid_file; // ~/.vnc/vnc.pid (TigerVNC)
        fs::path x_pid_file; // ~/.vnc/x.pid (X 进程 PID)

        // ── 日志 ──
        std::string vnc_log_file = "/tmp/tmoe_vnc_startup.log";

        // ── 依赖包名 (按发行版不同) ──
        std::string dep_viewer; // tigervnc-viewer
        std::string dep_server; // tigervnc-standalone-server
        std::string dep_extra; // xfonts-100dpi xfonts-75dpi xfonts-scalable

        /** 从当前环境初始化默认值。 */
        void init_defaults();

        /** 根据 display 编号自动计算 RFB 端口。 */
        void update_port();
    };

    /** 桌面环境元信息。
     *  对应 Bash 中各 install_xxx_desktop() 函数的 REMOTE_DESKTOP_SESSION_01/02 变量。
     */
    struct DesktopInfo {
        std::string id; // xfce/lxde/mate/kde/cinnamon/gnome/dde/ukui/budgie/cutefish/lxqt/openbox/i3/...
        std::string name; // 显示名称
        std::string emoji; // TUI 前缀 emoji (匹配旧 Bash 样式)
        std::string session_cmd1; // 第一候选会话命令 (如 xfce4-session)
        std::string session_cmd2; // 第二候选会话命令 (如 startxfce4)
        std::string display_manager; // 推荐的显示管理器 (lightdm/sddm/gdm)，WM 留空
        bool requires_root = false; // 是否需要 root 权限
        bool is_window_manager = false; // true=WM(窗口管理器), false=DE(完整桌面)
        std::string pkg_group; // 默认包名 (如 xfce4 xfce4-goodies)
        std::string compat_notes; // 兼容性说明
        // 发行版特定的包名覆盖: key=distro_family(debian/arch/redhat/void/gentoo/suse/alpine)
        std::map<std::string, std::string> distro_pkgs;
    };

    /** GUI / VNC 桌面环境管理。
     *  对应 Bash 脚本:
     *    - tools/gui/gui        (主 GUI 管理, ~5967 行)
     *    - tools/gui/startvnc   (VNC 启动)
     *    - tools/gui/stopvnc    (VNC 停止)
     *    - tools/gui/startx11vnc(x11vnc 启动)
     *    - tools/gui/novnc      (noVNC 启动)
     *    - tools/gui/install_novnc
     *    - tools/gui/configure_novnc
     *    - tools/gui/startxsdl  (XSDL/WSL)
     *    - tools/gui/wsl / wslg / wsl_pulse_audio
     *    - tools/gui/launch_dbus_daemon
     */
    class GUIManager {
    public:
        explicit GUIManager(const TmoeConfig &cfg);

        // ═══════════════════════════════════════════════
        // VNC 服务端安装与配置
        // ═══════════════════════════════════════════════

        /** 安装 VNC 服务端 (tigervnc + tightvnc + x11vnc)。 */
        bool install_vnc_server();

        /** 交互式选择 tiger 或 tight vnc 服务端。 */
        bool choose_vnc_server();

        /** 设置 VNC 密码 (6-8 位)。 */
        bool configure_vnc_password(std::string_view password = "");

        /** 写入 TigerVNC 默认配置文件。 */
        bool configure_vnc_defaults();

        /** 写入 ~/.vnc/xstartup 和 Xsession 会话脚本。 */
        bool configure_xstartup(std::string_view desktop);

        /** 检查 Xvnc 命令是否可用，不可用则安装。 */
        bool check_xvnc_command();

        // ═══════════════════════════════════════════════
        // VNC 服务启动/停止
        // ═══════════════════════════════════════════════

        /** 启动 VNC 服务 (tiger/tight)。
         *  @param display  display 编号，-1 使用配置默认值
         *  @param width    分辨率宽度，0 使用配置默认值
         *  @param height   分辨率高度，0 使用配置默认值
         */
        bool start_vnc(int display = -1, int width = 0, int height = 0);

        /** 停止 VNC 服务，清理锁文件和 socket。 */
        bool stop_vnc(int display = -1);

        /** 启动 x11vnc 服务 (基于 Xvfb 虚拟帧缓冲)。 */
        bool start_x11vnc(int display = -1);

        /** 停止 x11vnc 和 Xvfb。 */
        bool stop_x11vnc();

        /** 杀死所有 VNC 相关进程 (Xtightvnc, Xtigervnc, Xvnc)。 */
        bool kill_all_vnc();

        // ═══════════════════════════════════════════════
        // 桌面环境管理
        // ═══════════════════════════════════════════════

        /** 安装指定桌面环境。 */
        bool install_desktop(std::string_view desktop);

        /** 获取桌面环境元信息。 */
        DesktopInfo get_desktop_info(std::string_view desktop) const;

        /** 列出所有支持的桌面环境。 */
        std::vector<DesktopInfo> list_desktops() const;

        /** 安装窗口管理器 (openbox, icewm, i3, awesome, xmonad 等 70+)。 */
        bool install_window_manager(std::string_view wm);

        /** 列出所有支持的窗口管理器。 */
        std::vector<std::string> list_window_managers() const;

        // ═══════════════════════════════════════════════
        // 字体管理
        // ═══════════════════════════════════════════════

        /** 安装中文字体 (noto-cjk) + emoji 字体。 */
        bool install_fonts();

        /** 下载安装 Iosevka 编程字体。 */
        bool install_iosevka_font();

        // ═══════════════════════════════════════════════
        // HiDPI 检测与配置
        // ═══════════════════════════════════════════════

        /** 检测终端是否启用 HiDPI 并自动设置 WindowScalingFactor。 */
        bool detect_and_configure_hidpi(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // 输入法与浏览器
        // ═══════════════════════════════════════════════

        /** 安装 fcitx 中文拼音输入法。 */
        bool install_fcitx();

        // ═══════════════════════════════════════════════
        // 权限与文件管理
        // ═══════════════════════════════════════════════

        /** 修复 ~/.vnc 目录权限。 */
        bool fix_vnc_permissions();

        /** 部署 startvnc/startxsdl 启动脚本到 /usr/local/bin。 */
        bool deploy_startup_scripts();

        // ═══════════════════════════════════════════════
        // 网络与地址显示
        // ═══════════════════════════════════════════════

        /** 获取本机所有 IPv4/IPv6 地址。 */
        std::string get_local_ip_addresses() const;

        // ═══════════════════════════════════════════════
        // noVNC (HTML5 VNC 客户端)
        // ═══════════════════════════════════════════════

        /** 安装 noVNC 及其依赖 (websockify)。 */
        bool install_novnc();

        /** 配置 noVNC (端口/密码)。 */
        bool configure_novnc();

        /** 启动 noVNC websockify 代理。
         *  @param port  noVNC HTTP 端口，-1 使用默认 36080
         */
        bool start_novnc(int port = -1);

        /** 停止 noVNC websockify 并卸载。 */
        bool stop_novnc();

        /** 卸载 noVNC (pip3 + rm)。 */
        bool remove_novnc();

        /** 获取 noVNC 访问 URL。 */
        std::string get_novnc_url() const;

        // ═══════════════════════════════════════════════
        // XRDP (RDP 协议远程桌面)
        // ═══════════════════════════════════════════════

        /** 一键安装+配置 XRDP。 */
        bool install_xrdp();

        /** 配置 XRDP 会话 (修改 /etc/xrdp/startwm.sh)。 */
        bool configure_xrdp_session(std::string_view desktop);

        /** 启动 XRDP 服务。 */
        bool start_xrdp();

        /** 停止 XRDP 服务。 */
        bool stop_xrdp();

        /** 重启 XRDP 并显示连接信息。 */
        bool restart_xrdp();

        /** 修改 XRDP 端口。 */
        bool set_xrdp_port(int port);

        /** 卸载 XRDP。 */
        bool remove_xrdp();

        // ═══════════════════════════════════════════════
        // XSDL / VcXsrv / WSL
        // ═══════════════════════════════════════════════

        /** 配置 XSDL 连接参数。 */
        bool configure_xsdl();

        /** 启动 XSDL/VcXsrv (WSL 环境)。 */
        bool start_xsdl();

        /** 配置 WSL PulseAudio 桥接。 */
        bool configure_wsl_pulseaudio();

        /** 启动 WSLg (Xwayland)。 */
        bool start_wslg(int display_port = 2);

        // ═══════════════════════════════════════════════
        // D-Bus 守护进程
        // ═══════════════════════════════════════════════

        /** 启动 D-Bus 守护进程。 */
        bool launch_dbus_daemon();

        /** 修复 VNC dbus-launch 问题。 */
        bool fix_vnc_dbus();

        /** 停止 D-Bus 守护进程。 */
        bool stop_dbus_daemon();

        /** 显示 D-Bus 守护进程状态。 */
        void show_dbus_status() const;

        // ═══════════════════════════════════════════════
        // 桌面美化
        // ═══════════════════════════════════════════════

        /** 交互式桌面美化菜单。 */
        bool beautify_desktop();

        /** 安装桌面主题。 */
        bool install_theme(std::string_view theme);

        /** 安装图标主题。 */
        bool install_icon_theme(std::string_view theme);

        /** 设置桌面壁纸。 */
        bool set_wallpaper(std::string_view path);

        /** 下载壁纸 (支持多种 DE 官方壁纸)。 */
        bool download_wallpaper(std::string_view source);

        /** 安装 dock 栏 (plank)。 */
        bool install_dock();

        /** 安装 Conky 系统监控。 */
        bool install_conky();

        /** 安装 Compiz 窗口特效。 */
        bool install_compiz();

        /** 安装鼠标指针主题。 */
        bool install_cursor_theme(std::string_view theme);

        /** 部署 XFCE4 面板配置文件。 */
        bool deploy_xfce_panel_config();

        // ═══════════════════════════════════════════════
        // PulseAudio 音频桥接
        // ═══════════════════════════════════════════════

        /** 启动宿主机 PulseAudio TCP 桥接守护进程。 */
        bool start_pulseaudio_bridge() const;

        // ═══════════════════════════════════════════════
        // TUI 交互式菜单
        // ═══════════════════════════════════════════════

        /** GUI/VNC 管理主 TUI 菜单。 */
        void run_gui_menu();

        /** VNC 配置修改子菜单 (11 项)。 */
        void run_vnc_config_menu();

        /** 桌面环境安装子菜单。 */
        void run_desktop_install_menu();

        /** 远程桌面协议配置子菜单。 */
        void run_remote_desktop_menu();

        /** 桌面美化子菜单。 */
        void run_beautification_menu();

        /** 一键自动安装 GUI (Docker 环境用)。
         *  @param desktop  桌面环境 id (xfce/lxde/mate/kde/cutefish/dde/ukui)
         */
        bool auto_install_gui(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // 运行时状态查询
        // ═══════════════════════════════════════════════

        /** 检查 VNC 服务是否正在运行。 */
        bool is_vnc_running(int display = -1) const;

        /** 检测可用的 display 编号。 */
        int detect_available_display() const;

        /** 获取 VNC 连接 URI (vnc://127.0.0.1:5902)。 */
        std::string get_vnc_connection_uri() const;

        /** 获取 VNC 配置的可变引用 (供 Manager 使用)。 */
        VncConfig &vnc_config() { return vnc_config_; }
        const VncConfig &vnc_config() const { return vnc_config_; }

        // ═══════════════════════════════════════════════
        // CLI 标志处理
        // ═══════════════════════════════════════════════

        /** 处理来自 CLI 的 GUI 子命令标志。 */
        bool handle_gui_cli_flag(std::string_view flag);

        void set_auto_install_mode(bool v) { auto_install_mode_ = v; }
        bool is_auto_install() const { return auto_install_mode_; }

    private:
        const TmoeConfig &cfg_;
        VncConfig vnc_config_;
        int novnc_port_ = 36080;
        bool auto_install_mode_ = false;
        bool auto_install_fcitx4_ = false;
        bool auto_install_electron_ = false;
        bool auto_install_vscode_ = false;
        bool auto_install_chromium_ = true;
        bool auto_install_kali_ = false;
        std::string kali_tools_ = "kali-linux-arm";

        // ═══════════════════════════════════════════════
        // 文件 I/O 辅助
        // ═══════════════════════════════════════════════

        bool write_file(const fs::path &path, std::string_view content) const;

        bool append_file(const fs::path &path, std::string_view content) const;

        std::string read_file(const fs::path &path) const;

        // ═══════════════════════════════════════════════
        // 包管理辅助
        // ═══════════════════════════════════════════════

        bool install_packages(const std::vector<std::string> &packages) const;

        // ═══════════════════════════════════════════════
        // 桌面/窗口管理器注册表
        // ═══════════════════════════════════════════════

        const std::vector<DesktopInfo> &desktop_registry() const;

        const std::map<std::string, std::string> &window_manager_registry() const;

        // ═══════════════════════════════════════════════
        // VNC 安装子步骤
        // ═══════════════════════════════════════════════

        void tiger_vnc_variable();

        void tight_vnc_variable();

        void debian_remove_vnc_server();

        void debian_install_vnc_server();

        std::string grep_tiger_vnc_deb_file(const std::string &latest_deb_repo,
                                            const std::string &grep_name_01,
                                            const std::string &grep_name_02);

        void ubuntu_install_tiger_vnc_server();

        void case_debian_distro_and_install_vnc();

        void which_vnc_server_do_you_prefer();

        void modify_to_xfwm4_breeze_theme();

        void create_the_which_script();

        void check_the_which_command();

        void if_container_is_arm();

        void auto_select_keyboard_layout();

        void fix_mlocate();

        // ═══════════════════════════════════════════════
        // VNC 首次配置
        // ═══════════════════════════════════════════════

        void first_configure_vnc(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // VNC 命令构建
        // ═══════════════════════════════════════════════

        std::string build_vnc_start_command(int display, int width, int height) const;

        std::string build_xvfb_command(int display, int width, int height) const;

        std::string build_x11vnc_command(int display) const;

        // ═══════════════════════════════════════════════
        // 配置内容生成
        // ═══════════════════════════════════════════════

        std::string generate_xsession_content(std::string_view desktop) const;

        std::string generate_xstartup_content() const;

        std::string generate_xfce_panel_xml();

        std::string generate_xfce_desktop_xml();

        std::string generate_xfce_terminal_rc();

        std::string generate_budgie_desktop_builtin();

        std::string generate_gnome_flashback_metacity();

        std::string generate_gnome_session_classic();

        std::string generate_gnome_session_ubuntu();

        std::string generate_gnome_shell_x11();

        std::string generate_update_icon_caches_script();

        std::string generate_polkit_colord_conf();

        std::string generate_polkit_colord_pkla();

        // ═══════════════════════════════════════════════
        // VNC PID 管理
        // ═══════════════════════════════════════════════

        void write_vnc_pid_file(int display) const;

        void remove_vnc_pid_file(int display) const;

        // ═══════════════════════════════════════════════
        // 环境检测
        // ═══════════════════════════════════════════════

        bool detect_android_resolution(int &width, int &height) const;

        void detect_wsl_environment();

        // ═══════════════════════════════════════════════
        // 桌面安装前置/后置处理
        // ═══════════════════════════════════════════════

        void preconfigure_gui_dependencies();

        void will_be_installed_for_you(std::string_view desktop_session);

        void select_kali_tools();

        void post_desktop_install_prompts();

        void plasma_wayland_env();

        void post_install_desktop_config(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // TUI 子菜单 (桌面/窗口管理器相关)
        // ═══════════════════════════════════════════════

        void run_rootless_de_menu();

        void run_rootful_de_menu();

        void after_desktop_install_hint();

        void run_wm_menu();

        void run_dm_menu();

        // ═══════════════════════════════════════════════
        // TUI 子菜单 (远程桌面配置相关)
        // ═══════════════════════════════════════════════

        void run_x11vnc_config_menu();

        void run_novnc_config_menu();

        void run_xsdl_config_menu();

        void run_xrdp_menu();

        void configure_remote_desktop_environment(std::string_view context);

        void configure_xrdp_desktop();

        void configure_x11vnc_remote_desktop_session();

        void configure_xrdp_remote_desktop_session(const std::string &session_cmd);

        // ═══════════════════════════════════════════════
        // x11vnc 配置修改子步骤
        // ═══════════════════════════════════════════════

        void modify_x11vnc_pulse_server();

        void modify_x11vnc_resolution();

        void modify_x11vnc_port();

        // ═══════════════════════════════════════════════
        // Docker 自动安装辅助
        // ═══════════════════════════════════════════════

        void docker_auto_install_gui_env(std::string_view /*desktop*/);

        // ═══════════════════════════════════════════════
        // 字体下载辅助
        // ═══════════════════════════════════════════════

        void download_iosevka_ttf_font_ext();

        // ═══════════════════════════════════════════════
        // GNOME / Budgie 桌面会话配置
        // ═══════════════════════════════════════════════

        void set_gnome_common_env();

        void ln_s_gnome_flashback_metacity();

        void set_gnome_desktop_deps();

        void get_ubuntu_desktop_language_pack();

        void set_budgie_desktop_session(const std::string &session_type);

        // ═══════════════════════════════════════════════
        // XFCE4 终端配色方案
        // ═══════════════════════════════════════════════

        void touch_xfce4_terminal_rc_ext();

        void xfce4_color_scheme();

        // ═══════════════════════════════════════════════
        // 启动脚本配置
        // ═══════════════════════════════════════════════

        void configure_startvnc();

        void configure_startxsdl();

        // ═══════════════════════════════════════════════
        // 权限修复
        // ═══════════════════════════════════════════════

        void fix_non_root_permissions();

        // ═══════════════════════════════════════════════
        // 主题管理子步骤
        // ═══════════════════════════════════════════════

        void configure_theme_menu();

        void xfce_theme_parsing();

        void local_theme_installer();

        void check_theme_url(std::string &url);

        void tmoe_theme_installer(const std::string &file_path, bool is_icon);

        // ═══════════════════════════════════════════════
        // 特定主题下载/安装
        // ═══════════════════════════════════════════════

        void install_kali_undercover();

        void download_macos_mojave_theme();

        void download_macos_bigsur_theme();

        void install_breeze_theme_ext();

        void install_arc_gtk_theme_ext();

        void install_moka_theme_ext();

        void install_numix_theme_ext();

        // ═══════════════════════════════════════════════
        // 图标主题辅助
        // ═══════════════════════════════════════════════

        void set_default_xfce_icon_theme(const std::string &icon_name);

        void create_update_icon_caches();

        void check_update_icon_caches_sh();

        // ═══════════════════════════════════════════════
        // Kali 主题
        // ═══════════════════════════════════════════════

        void git_clone_kali_themes_common();

        void download_kali_themes_common();

        void download_kali_theme();

        // ═══════════════════════════════════════════════
        // 鼠标指针主题
        // ═══════════════════════════════════════════════

        void download_arch_breeze_adapta_cursor_theme();

        void configure_mouse_cursor();

        void download_chameleon_cursor_theme();

        // ═══════════════════════════════════════════════
        // 图标主题下载菜单与单项下载
        // ═══════════════════════════════════════════════

        void download_icon_themes_menu();

        void download_win10x_theme();

        void download_candy_icon_theme();

        void download_paper_icon_theme();

        void download_papirus_icon_theme();

        void download_raspbian_pixel_icon_theme();

        void download_uos_icon_theme();

        // ═══════════════════════════════════════════════
        // WSL 组件下载 / 预览
        // ═══════════════════════════════════════════════

        void download_wsl_components();

        void catimg_preview_lxde_mate_xfce();

        // ═══════════════════════════════════════════════
        // x11vnc 辅助
        // ═══════════════════════════════════════════════

        void x11vnc_warning();

        void x11vnc_onekey();

        void remove_x11vnc_ext();

        void x11vnc_doc();

        void x11vnc_process_readme();

        // ═══════════════════════════════════════════════
        // VNC 配置修改
        // ═══════════════════════════════════════════════

        void check_vnc_resolution();

        void modify_vnc_conf();

        // ═══════════════════════════════════════════════
        // XRDP 辅助
        // ═══════════════════════════════════════════════

        void xrdp_onekey();

        void check_xrdp_status();

        void xrdp_pulse_server();

        void xrdp_systemd();

        void xrdp_reset();

        void xrdp_restart();

        void xrdp_port();

        // ═══════════════════════════════════════════════
        // 桌面环境警告/提示
        // ═══════════════════════════════════════════════

        void xfce_warning();

        void kde_warning();

        void gnome3_warning();

        void cinnamon_warning();

        void deepin_desktop_warning();

        void dde_warning();

        void arch_linux_mate_warning();

        void tmoe_desktop_warning();

        // ═══════════════════════════════════════════════
        // 其他提示/FAQ
        // ═══════════════════════════════════════════════

        void print_gnome_ascii();

        void tips_of_tiger_vnc_server();

        void tmoe_desktop_faq();

        void do_you_want_to_configure_novnc();

        // ═══════════════════════════════════════════════
        // Deepin / DDE / Ubuntu DDE 辅助
        // ═══════════════════════════════════════════════

        void ubuntu_dde_distro_code(std::string &target_code);

        void deepin_desktop_debian();

        void ubuntu_dde_or_dde_extras(std::string &dep_01);

        void fix_dde_dpkg_error_ext();

        // ═══════════════════════════════════════════════
        // 壁纸设置辅助
        // ═══════════════════════════════════════════════

        void modify_the_default_xfce_wallpaper();

        void modify_xfce_vnc0_wallpaper(const std::string &path);

        void xfce_papirus_icon_theme_ext();

        void debian_xfce_wallpaper();

        void debian_download_mint_wallpaper();

        void debian_download_ubuntu_mate_wallpaper_ext();

        void debian_download_xubuntu_xenial_wallpaper_ext();

        void set_linuxmint_wallpaper_vars(const std::string &mint_code,
                                          std::string &out_name, std::string &out_path);

        // ═══════════════════════════════════════════════
        // 壁纸下载菜单与工具
        // ═══════════════════════════════════════════════

        std::string pick_random_wallpaper_pack();

        void download_wallpapers_menu();

        void ubuntu_wallpapers_and_photos_menu();

        void ubuntu_gnome_wallpapers_menu();

        void xubuntu_wallpapers_menu();

        void linux_mint_backgrounds_menu();

        // ═══════════════════════════════════════════════
        // 各发行版/DE 壁纸下载
        // ═══════════════════════════════════════════════

        void download_ubuntu_wallpaper(const std::string &ubuntu_code);

        void download_xubuntu_wallpaper(const std::string &code_name, const std::string & /*folder_name*/);

        void download_mint_backgrounds(const std::string &mint_code);

        void download_deepin_wallpaper();

        void download_elementary_wallpaper();

        void download_manjaro_wallpaper();

        void download_debian_gnome_wallpaper();

        void download_arch_wallpaper();

        void download_arch_xfce_artwork();

        void download_raspbian_pixel_wallpaper();

        void download_ubuntu_mate_wallpaper();

        void download_ubuntu_kylin_wallpaper();

        void link_to_debian_wallpaper();

        void download_manjaro_pkg(const std::string &theme_name,
                                  const std::string &url,
                                  const std::string &url_02,
                                  const std::string & /*wallpaper_name*/,
                                  const std::string & /*custom_name*/);

        // ═══════════════════════════════════════════════
        // FVWM 安装
        // ═══════════════════════════════════════════════

        void install_fvwm_ext();

        // ═══════════════════════════════════════════════
        // 显示管理器 systemctl 操作
        // ═══════════════════════════════════════════════

        void tmoe_display_manager_systemctl(const std::string &dm_pkg, const std::string &dm_service);

        // ═══════════════════════════════════════════════
        // 杂项辅助
        // ═══════════════════════════════════════════════

        void check_zstd();

        void random_neko();

        void download_and_cat_icon_img(const std::string &url, const std::string &filename);

        void check_tmoe_linux_desktop_link();

        void install_gui();

        // ═══════════════════════════════════════════════
        // Plasma / VNC 最终配置辅助
        // ═══════════════════════════════════════════════

        void choose_plasma_wayland_or_x11();

        void set_vnc_passwd();

        void choose_vnc_port_5902_or_5903();

        void fix_vnc_dbus_launch();
    };
} // namespace tmoe::domain
