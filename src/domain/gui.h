#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/command_builder.hpp"
#include "gui_config/templates.h"
#include "gui_config/registries.h"
#include "vnc_manager.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

namespace tmoe::domain {

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
 *
 *  VNC 核心功能已委托给 VncManager。
 */
class GUIManager {
public:
    explicit GUIManager(const TmoeConfig &cfg);

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
    // 网络与地址显示 (委托给 VncManager)
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
    // CLI 标志处理
    // ═══════════════════════════════════════════════

    /** 处理来自 CLI 的 GUI 子命令标志。 */
    bool handle_gui_cli_flag(std::string_view flag);

    void set_auto_install_mode(bool v) { auto_install_mode_ = v; }
    bool is_auto_install() const { return auto_install_mode_; }

    // ═══════════════════════════════════════════════
    // VncManager 访问 (供 Manager 等使用)
    // ═══════════════════════════════════════════════

    VncManager& vnc_manager() { return vnc_manager_; }
    const VncManager& vnc_manager() const { return vnc_manager_; }

private:
    const TmoeConfig &cfg_;
    VncManager vnc_manager_;
    int novnc_port_ = 36080;
    bool auto_install_mode_ = false;
    bool auto_install_fcitx4_ = false;
    bool auto_install_electron_ = false;
    bool auto_install_vscode_ = false;
    bool auto_install_chromium_ = true;
    bool auto_install_kali_ = false;
    std::string kali_tools_ = "kali-linux-arm";

    // ═══════════════════════════════════════════════
    // 桌面/窗口管理器注册表
    // ═══════════════════════════════════════════════

    const std::vector<DesktopInfo> &desktop_registry() const;

    const std::map<std::string, std::string> &window_manager_registry() const;

    // ═══════════════════════════════════════════════
    // VNC 首次配置 (协调者，委托给 VncManager)
    // ═══════════════════════════════════════════════

    void first_configure_vnc(std::string_view desktop);

    // ═══════════════════════════════════════════════
    // 配置内容生成 (桌面会话相关，非 VNC)
    // ═══════════════════════════════════════════════

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
    // x11vnc 辅助 (留在 GUIManager)
    // ═══════════════════════════════════════════════

    void x11vnc_warning();

    void x11vnc_onekey();

    void remove_x11vnc_ext();

    void x11vnc_doc();

    void x11vnc_process_readme();

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
    // Plasma / VNC 最终配置辅助 (CLI 标志处理)
    // ═══════════════════════════════════════════════

    void choose_plasma_wayland_or_x11();

    void set_vnc_passwd();

    void choose_vnc_port_5902_or_5903();

    void fix_vnc_dbus_launch();
};
} // namespace tmoe::domain
