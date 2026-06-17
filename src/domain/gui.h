#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/command_builder.hpp"
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

        // ── 安全 ──
        std::string password; // VNC 密码 (6-8 位)
        fs::path passwd_file; // 密码文件路径 (~/.vnc/passwd)
        bool always_shared = true; // 总是共享连接

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
        std::string id; // xfce/lxde/mate/kde/cinnamon/gnome/dde/ukui/budgie/cutefish/lxqt
        std::string name; // 显示名称
        std::string session_cmd1; // 第一候选会话命令 (如 xfce4-session)
        std::string session_cmd2; // 第二候选会话命令 (如 startxfce4)
        std::string display_manager; // 推荐的显示管理器 (lightdm/sddm/gdm)
        bool requires_root = false; // 是否需要 root 权限
        std::string pkg_group; // apt 包组名 (如 xfce4 xfce4-goodies)
        std::string compat_notes; // 兼容性说明
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

        /** 安装窗口管理器 (openbox, icewm, i3, awesome, xmonad 等 50+)。 */
        bool install_window_manager(std::string_view wm);

        /** 列出所有支持的窗口管理器。 */
        std::vector<std::string> list_window_managers() const;

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

        /** 安装 dock 栏 (plank)。 */
        bool install_dock();

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

    private:
        const TmoeConfig &cfg_;
        VncConfig vnc_config_;
        int novnc_port_ = 36080;

        // ── 内部辅助 ──
        bool write_file(const fs::path &path, std::string_view content) const;

        bool append_file(const fs::path &path, std::string_view content) const;

        std::string read_file(const fs::path &path) const;

        // ── VNC 命令构建 ──
        std::string build_vnc_start_command(int display, int width, int height) const;

        std::string build_x11vnc_command(int display) const;

        std::string build_xvfb_command(int display, int width, int height) const;

        // ── 配置生成 ──
        std::string generate_xsession_content(std::string_view desktop) const;

        std::string generate_xstartup_content() const;

        // ── 环境检测 ──
        bool detect_android_resolution(int &width, int &height) const;

        void detect_wsl_environment();

        // ── 包管理辅助 ──
        bool install_packages(const std::vector<std::string> &packages) const;

        // ── 桌面环境注册表 ──
        static const std::vector<DesktopInfo> &desktop_registry();

        static const std::map<std::string, std::string> &window_manager_registry();
    };
} // namespace tmoe::domain
