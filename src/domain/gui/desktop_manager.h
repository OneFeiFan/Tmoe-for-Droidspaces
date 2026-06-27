#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include "core/config.h"
#include "vnc_manager.h"
#include "../gui_config/registries.h"
#include "../gui_config/templates.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"

namespace fs = std::filesystem;

namespace tmoe::domain {
    class DesktopManager {
    public:
        DesktopManager(const TmoeConfig &cfg, VncManager &vnc_manager);

        // ---------- 注册表查询 ----------
        const std::vector<DesktopInfo> &desktop_registry() const;

        DesktopInfo get_desktop_info(std::string_view desktop) const;

        std::vector<DesktopInfo> list_desktops() const;

        std::vector<std::string> list_window_managers() const;

        // ---------- 安装 ----------
        bool install_desktop(std::string_view desktop);

        bool install_window_manager(std::string_view wm);

        // ---------- 安装前后置 ----------
        void preconfigure_gui_dependencies();

        void will_be_installed_for_you(std::string_view desktop_session);

        void post_desktop_install_prompts();

        void execute_optional_installs();

        // ---------- 输入法 ----------
        bool install_fcitx();

        void after_desktop_install_hint() const;

        void select_kali_tools();

        void install_fvwm_ext();

        void tmoe_display_manager_systemctl(const std::string &dm_pkg, const std::string &dm_service);

        // ---------- 字体（被 preconfigure_gui_dependencies 和 install_gui 调用） ----------
        bool install_fonts();

        void download_iosevka_ttf_font_ext();

        // ---------- 主题/图标/壁纸（被 post_install 调用） ----------
        void install_kali_undercover();

        void download_macos_mojave_theme();

        void download_macos_bigsur_theme();

        void install_breeze_theme_ext();

        void install_arc_gtk_theme_ext();

        void install_moka_theme_ext();

        void install_numix_theme_ext();

        void download_kali_theme();

        void set_default_xfce_icon_theme(const std::string &icon_name);

        void create_update_icon_caches();

        void check_update_icon_caches_sh();

        void git_clone_kali_themes_common();

        void download_kali_themes_common();

        void download_arch_breeze_adapta_cursor_theme();

        void debian_download_mint_wallpaper();

        void debian_download_ubuntu_mate_wallpaper_ext();

        void debian_download_xubuntu_xenial_wallpaper_ext();

        void modify_the_default_xfce_wallpaper();

        void modify_xfce_vnc0_wallpaper(const std::string &path);

        void xfce_papirus_icon_theme_ext();

        void debian_xfce_wallpaper();

        // ---------- 壁纸下载 ----------
        void download_mint_backgrounds(const std::string &mint_code);

        void download_ubuntu_mate_wallpaper();

        void download_xubuntu_wallpaper(const std::string &code_name, const std::string &folder_name);

        void set_linuxmint_wallpaper_vars(const std::string &mint_code, std::string &out_name, std::string &out_path);

        std::string pick_random_wallpaper_pack();

        // ---------- 内容生成（用于桌面后配置） ----------
        std::string generate_xfce_desktop_xml();

        std::string generate_xfce_terminal_rc();

        std::string generate_budgie_desktop_builtin();

        std::string generate_gnome_flashback_metacity();

        std::string generate_gnome_session_classic();

        std::string generate_gnome_session_ubuntu();

        std::string generate_gnome_shell_x11();

        std::string generate_update_icon_caches_script();

        // ---------- 提示/图标 ----------
        void print_gnome_ascii() const;

        void download_papirus_icon_theme();

        // ---------- 自动安装标志 ----------
        void set_auto_install_flags(bool fcitx, bool electron, bool vscode, bool chromium, bool kali,
                                    const std::string &kali_tools);

        void set_auto_install_mode(bool v) { auto_install_mode_ = v; }
        bool is_auto_install_mode() const { return auto_install_mode_; }

        // 自动安装标志 getter（供 post_desktop_install_prompts 等使用）
        bool auto_fcitx() const { return auto_install_fcitx4_; }
        bool auto_electron() const { return auto_install_electron_; }
        bool auto_vscode() const { return auto_install_vscode_; }
        bool auto_chromium() const { return auto_install_chromium_; }
        bool auto_kali() const { return auto_install_kali_; }
        const std::string &kali_tools() const { return kali_tools_; }

    private:
        const TmoeConfig &cfg_;
        VncManager &vnc_manager_;

        bool auto_install_mode_ = false;
        bool auto_install_fcitx4_ = false;
        bool auto_install_electron_ = false;
        bool auto_install_vscode_ = false;
        bool auto_install_chromium_ = false;  // 默认不装，由用户选择
        bool auto_install_kali_ = false;
        std::string kali_tools_ = "kali-linux-arm";

        /// 解析当前发行版家族 (优先从 config，回退到运行时检测)
        DistroFamily resolved_family() const;

        /// 使用 PackageManager 安装包列表
        bool install_packages(const std::vector<std::string> &pkgs) const;

        /// 使用 PackageManager 卸载包列表
        bool remove_packages(const std::vector<std::string> &pkgs) const;

    };
} // namespace tmoe::domain
