#pragma once

#include <string>
#include <string_view>
#include "core/config.h"

namespace tmoe::domain {
    class DesktopManager;

    /** 桌面美化管理器 — 主题/壁纸/图标/光标/Dock/Conky/Compiz。
     *  从 GUIManager 独立出来，消除 gui.cpp 臃肿。 */
    class BeautificationManager {
    public:
        explicit BeautificationManager(const TmoeConfig &cfg, DesktopManager &dm);

        // ── 菜单入口 ──
        void run_beautification_menu();

        // ── 主题 ──
        void configure_theme_menu();

        bool install_theme(std::string_view theme) const;

        void tmoe_theme_installer(const std::string &file_path, bool is_icon);

        void local_theme_installer();

        void xfce_theme_parsing();

        // ── 图标主题 ──
        void download_icon_themes_menu();

        bool install_icon_theme(std::string_view theme) const;

        void download_paper_icon_theme();

        void download_candy_icon_theme();

        void download_uos_icon_theme();

        void download_raspbian_pixel_assets();

        // ── 壁纸 ──
        void download_wallpapers_menu();

        void ubuntu_wallpapers_and_photos_menu();

        void xubuntu_wallpapers_menu();

        void linux_mint_backgrounds_menu();

        void ubuntu_gnome_wallpapers_menu();

        bool download_wallpaper(std::string_view source);

        void download_ubuntu_wallpaper(const std::string &ubuntu_code);

        void download_deepin_wallpaper();

        void download_elementary_wallpaper();

        void download_arch_wallpaper();

        void download_manjaro_wallpaper();

        void download_arch_xfce_artwork();

        void download_debian_gnome_wallpaper();

        void download_ubuntu_kylin_wallpaper();

        void link_to_debian_wallpaper();

        bool set_wallpaper(std::string_view path);

        // ── 光标 / 鼠标 ──
        bool install_cursor_theme(std::string_view theme);

        void configure_mouse_cursor();

        void download_chameleon_cursor_theme();

        // ── 面板 / Dock / Conky / Compiz ──
        bool beautify_desktop();

        void configure_xfce_terminal_colors();

        bool install_dock() const;

        bool install_conky();

        void configure_conky();

        bool install_compiz();

        bool deploy_xfce_panel_config();

        std::string generate_xfce_panel_xml();

        // ── 主题下载 ──
        void download_win10x_theme();

        void download_manjaro_pkg(const std::string &theme_name, const std::string &url,
                                  const std::string &url_02 = "",
                                  const std::string &wallpaper_name = "",
                                  const std::string &custom_name = "");

        // ── 工具 ──
        void download_and_cat_icon_img(const std::string &url, const std::string &filename);

        void catimg_preview_lxde_mate_xfce();

        void check_theme_url(std::string &url);

    private:
        const TmoeConfig &cfg_;
        DesktopManager &desktop_manager_;
    };
} // namespace tmoe::domain
