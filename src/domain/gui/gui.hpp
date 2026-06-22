#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/command_builder.hpp"
#include "../gui_config/templates.h"
#include "../gui_config/registries.h"
#include "vnc_manager.h"
#include "desktop_manager.h"
#include "remote_desktop_manager.h"
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include "vnc_manager.h"
#include "core/system_helper.h"
#include "core/logger.h"
#include "core/executor.h"
#include "core/config.h"
#include "core/i18n.h"
#include "../gui_config/templates.h"
#include "../gui_config/registries.h"
#include "../package_manager.h"

#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace tmoe::domain {
    class GUIManager {
    public:
        explicit GUIManager(const TmoeConfig &cfg);

        // ═══════════════════════════════════════════════
        // 网络与地址显示
        // ═══════════════════════════════════════════════

        std::string get_local_ip_addresses() const;

        // ═══════════════════════════════════════════════
        // XSDL / VcXsrv / WSL
        // ═══════════════════════════════════════════════

        bool configure_xsdl();

        bool start_xsdl();

        bool configure_wsl_pulseaudio();

        bool start_wslg(int display_port = 2);

        // ═══════════════════════════════════════════════
        // 桌面美化
        // ═══════════════════════════════════════════════

        bool beautify_desktop();

        bool install_theme(std::string_view theme);

        bool install_icon_theme(std::string_view theme);

        bool set_wallpaper(std::string_view path);

        bool download_wallpaper(std::string_view source);

        bool install_dock();

        bool install_conky();

        bool install_compiz();

        bool install_cursor_theme(std::string_view theme);

        bool deploy_xfce_panel_config();

        // ═══════════════════════════════════════════════
        // PulseAudio 音频桥接
        // ═══════════════════════════════════════════════

        bool start_pulseaudio_bridge() const;

        // ═══════════════════════════════════════════════
        // 字体管理
        // ═══════════════════════════════════════════════

        bool install_iosevka_font();

        // ═══════════════════════════════════════════════
        // HiDPI 检测与配置
        // ═══════════════════════════════════════════════

        bool detect_and_configure_hidpi(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // TUI 交互式菜单
        // ═══════════════════════════════════════════════

        void run_gui_menu();

        void run_vnc_config_menu();

        void run_desktop_install_menu();

        void run_remote_desktop_menu();

        void run_beautification_menu();

        // ═══════════════════════════════════════════════
        // 一键自动安装 GUI
        // ═══════════════════════════════════════════════

        bool auto_install_gui(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // CLI 标志处理
        // ═══════════════════════════════════════════════

        bool handle_gui_cli_flag(std::string_view flag);

        void set_auto_install_mode(bool v) { auto_install_mode_ = v; }
        bool is_auto_install() const { return auto_install_mode_; }

        // ═══════════════════════════════════════════════
        // 组件访问
        // ═══════════════════════════════════════════════

        VncManager &vnc_manager() { return vnc_manager_; }
        const VncManager &vnc_manager() const { return vnc_manager_; }
        DesktopManager &desktop_manager() { return desktop_manager_; }
        const DesktopManager &desktop_manager() const { return desktop_manager_; }
        RemoteDesktopManager &remote_desktop_manager() { return remote_desktop_manager_; }
        const RemoteDesktopManager &remote_desktop_manager() const { return remote_desktop_manager_; }

    private:
        const TmoeConfig &cfg_;
        VncManager vnc_manager_;
        DesktopManager desktop_manager_;
        RemoteDesktopManager remote_desktop_manager_;
        bool auto_install_mode_ = false;

        // ═══════════════════════════════════════════════
        // VNC 首次配置
        // ═══════════════════════════════════════════════

        void first_configure_vnc(std::string_view desktop);

        // ═══════════════════════════════════════════════
        // 配置内容生成
        // ═══════════════════════════════════════════════

        std::string generate_xfce_panel_xml();

        // ═══════════════════════════════════════════════
        // 桌面安装辅助 (仍在 GUIManager)
        // ═══════════════════════════════════════════════

        void docker_auto_install_gui_env(std::string_view /*desktop*/);

        void install_gui();

        // ═══════════════════════════════════════════════
        // TUI 子菜单
        // ═══════════════════════════════════════════════

        void run_rootless_de_menu();

        void run_rootful_de_menu();

        void run_wm_menu();

        void run_dm_menu();

        void run_xsdl_config_menu();

        // ═══════════════════════════════════════════════
        // 主题管理子步骤
        // ═══════════════════════════════════════════════

        void configure_theme_menu();

        void xfce_theme_parsing();

        void local_theme_installer();

        void check_theme_url(std::string &url);

        void tmoe_theme_installer(const std::string &file_path, bool is_icon);

        // ═══════════════════════════════════════════════
        // 图标主题下载菜单与单项下载
        // ═══════════════════════════════════════════════

        void download_icon_themes_menu();

        void download_win10x_theme();

        void download_candy_icon_theme();

        void download_paper_icon_theme();

        void download_raspbian_pixel_icon_theme();

        void download_uos_icon_theme();

        // ═══════════════════════════════════════════════
        // WSL 组件下载 / 预览
        // ═══════════════════════════════════════════════

        void download_wsl_components();

        void catimg_preview_lxde_mate_xfce();

        // ═══════════════════════════════════════════════
        // 壁纸设置辅助
        // ═══════════════════════════════════════════════

        void download_wallpapers_menu();

        void ubuntu_wallpapers_and_photos_menu();

        void ubuntu_gnome_wallpapers_menu();

        void xubuntu_wallpapers_menu();

        void linux_mint_backgrounds_menu();

        void download_ubuntu_wallpaper(const std::string &ubuntu_code);

        void download_deepin_wallpaper();

        void download_elementary_wallpaper();

        void download_manjaro_wallpaper();

        void download_debian_gnome_wallpaper();

        void download_arch_wallpaper();

        void download_arch_xfce_artwork();

        void download_raspbian_pixel_wallpaper();

        void download_ubuntu_kylin_wallpaper();

        void link_to_debian_wallpaper();

        void download_manjaro_pkg(const std::string &theme_name,
                                  const std::string &url,
                                  const std::string &url_02,
                                  const std::string & /*wallpaper_name*/,
                                  const std::string & /*custom_name*/);

        // ═══════════════════════════════════════════════
        // 其他辅助
        // ═══════════════════════════════════════════════

        void ensure_tmoe_symlink();

        void configure_mouse_cursor();

        void download_chameleon_cursor_theme();

        void check_zstd();

        void random_neko();

        void download_and_cat_icon_img(const std::string &url, const std::string &filename);

        void check_tmoe_linux_desktop_link();

        // ═══════════════════════════════════════════════
        // VNC 最终配置辅助 (CLI 标志处理)
        // ═══════════════════════════════════════════════

        void set_vnc_passwd();

        void choose_vnc_port_5902_or_5903();

        void fix_vnc_dbus_launch();
    };
} // namespace tmoe::domain
