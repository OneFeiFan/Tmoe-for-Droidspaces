#include "beautification_manager.h"
#include "desktop_manager.h"
#include "../gui_config/templates.h"
#include "domain/system/package_manager.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "core/command_builder.hpp"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "core/str_utils.h"
#include <filesystem>
#include <regex>
#include <cstring>

namespace fs = std::filesystem;

namespace tmoe::domain {
    namespace {
        DistroFamily get_family(const TmoeConfig &cfg) {
            return PackageManager::resolve_family(cfg.linux_distro);
        }
    } // anonymous namespace

    // ═══════════════════════════════════════════════════════════════
    // 构造
    // ═══════════════════════════════════════════════════════════════

    BeautificationManager::BeautificationManager(const TmoeConfig &cfg, DesktopManager &dm)
        : cfg_(cfg), desktop_manager_(dm) {}

    // ═══════════════════════════════════════════════════════════════
    // 顶层菜单
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::run_beautification_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(
            _("gui.beautify_title"), _("gui.beautify_prompt"), "beautification");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.themes"), "1",
            [this](MenuContext&) -> bool {
                configure_theme_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.icon_theme"), "2",
            [this](MenuContext&) -> bool {
                download_icon_themes_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.wallpaper"), "3",
            [this](MenuContext&) -> bool {
                download_wallpapers_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.mouse_cursor"), "4",
            [this](MenuContext&) -> bool {
                configure_mouse_cursor();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.dock"), "5",
            [this](MenuContext&) -> bool {
                install_dock();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.beautify.compiz"), "6",
            [this](MenuContext&) -> bool {
                install_compiz();
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    // ═══════════════════════════════════════════════════════════════
    // 基础操作
    // ═══════════════════════════════════════════════════════════════

    bool BeautificationManager::install_icon_theme(std::string_view theme) const {
        Logger::step(std::string(_("gui.beautify.install_icon_theme")) + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::icon_theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool BeautificationManager::install_theme(std::string_view theme) const {
        Logger::step(std::string(_("gui.beautify.install_theme")) + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool BeautificationManager::set_wallpaper(std::string_view path) {
        Logger::step(_("gui.beautify.set_wallpaper"));
        if (!path.empty() && fs::exists(path)) {
            Executor::shell(CommandBuilder("xfconf-query")
                            .add_arg("-c").add_arg("xfce4-desktop")
                            .add_arg("-p").add_arg("/backdrop/screen0/monitor0/workspace0/last-image")
                            .add_arg("-s").add_arg(std::string(path))
                            .build_string() + " 2>/dev/null &");
            Logger::ok(_("gui.beautify.wallpaper_set"));
            return true;
        }
        Logger::info(_("gui.beautify.wallpaper_manual_hint"));
        Logger::info(
            "  xfconf-query -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image -s /path/to/wallpaper.jpg");
        return true;
    }

    bool BeautificationManager::beautify_desktop() {
        Logger::step(_("gui.beautify_step"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // XFCE 终端配色 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::configure_xfce_terminal_colors() {
        Logger::step(_("gui.xfce_terminal.color_step"));

        // 1. 下载 Monokai Remastered 配色方案
        std::string colorscheme_dir = "/usr/share/xfce4/terminal/colorschemes";
        if (!fs::exists(colorscheme_dir + "/Monokai Remastered.theme")) {
            fs::create_directories(colorscheme_dir);
            std::string tmp_arc = "/tmp/.tmoe_colorschemes.tar.xz";
            SystemHelper::download_file("https://gitee.com/mo2/xfce-themes/raw/terminal/colorschemes.tar.xz", tmp_arc);
            if (fs::exists(tmp_arc)) {
                // tar -Jxf 保留原始权限
                Executor::passthrough("cd '" + colorscheme_dir + "' && sudo tar -Jxf '" + tmp_arc + "' 2>/dev/null || true");
                std::error_code ec;
                fs::remove(tmp_arc, ec);
            }
        }

        // 2. 创建 ~/.config/xfce4/terminal/terminalrc（使用统一模板）
        std::string home = SystemHelper::user_home();
        fs::path terminal_dir = fs::path(home) / ".config" / "xfce4" / "terminal";
        fs::path terminalrc = terminal_dir / "terminalrc";

        if (!fs::exists(terminalrc)) {
            fs::create_directories(terminal_dir);
            SystemHelper::write_file(terminalrc, gui_config::XFCE_TERMINAL_RC);
        }

        // 3. 补全 ColorPalette（如果缺失）
        std::string rc_str = SystemHelper::read_file(terminalrc);
        if (rc_str.find("ColorPalette") == std::string::npos) {
            SystemHelper::append_file(terminalrc,
                "\nColorPalette=#000000;#ff3333;#b8cc52;#e7c547;#36a3d9;#f07178;#95e6cb;#ffffff;#323232;#ff6565;#eafe84;#fff779;#68d5ff;#ffa3aa;#c7fffd;#ffffff\n"
                "ColorForeground=#e6e1cf\nColorBackground=#0f1419\n");
        }

        // 4. 设置等宽字体
        if (rc_str.find("FontName") == std::string::npos) {
            std::string font_line;
            if (fs::exists("/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf"))
                font_line = "\nFontName=Iosevka Term Bold 12\n";
            else if (fs::exists("/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc"))
                font_line = "\nFontName=Noto Sans Mono CJK SC Bold 12\n";
            if (!font_line.empty())
                SystemHelper::append_file(terminalrc, font_line);
        }

        Logger::ok(_("gui.xfce_terminal.color_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // Dock / Conky / Compiz
    // ═══════════════════════════════════════════════════════════════

    bool BeautificationManager::install_dock() const {
        Logger::step(_("gui.dock.install_step"));
        if (!PackageManager::install("plank", get_family(cfg_))) {
            Logger::warn(_("gui.dock.install_failed"));
            return false;
        }

        std::string home = SystemHelper::user_home();
        std::string autostart_dir = home + "/.config/autostart";
        fs::create_directories(autostart_dir);
        SystemHelper::write_file(fs::path(autostart_dir + "/plank.desktop"),
            "[Desktop Entry]\nType=Application\nName=Plank\nExec=plank\n"
            "X-GNOME-Autostart-enabled=true\nOnlyShowIn=XFCE;LXDE;MATE;Budgie;\n");
        Logger::info(_("gui.dock.autostart_created"));

        std::string themes_dir = home + "/.local/share/plank/themes";
        if (!fs::exists(themes_dir + "/anti-snap")) {
            fs::create_directories(themes_dir);
            std::string tmp_clone = "/tmp/.plank_anti_snap_theme";
            std::error_code ec;
            fs::remove_all(tmp_clone, ec);
            // git clone 保留仓库原始文件权限
            auto clone1 = CommandBuilder("git").add_arg("clone").add_flag("--depth=1")
                .add_arg("https://github.com/TaylanTatli/Anti-Snap.git").add_arg(tmp_clone)
                .add_raw("2>/dev/null").build_string();
            auto clone2 = CommandBuilder("git").add_arg("clone").add_flag("--depth=1")
                .add_arg("git://github.com/TaylanTatli/Anti-Snap.git").add_arg(tmp_clone)
                .add_raw("2>/dev/null").build_string();
            Executor::passthrough("cd /tmp && (" + clone1 + " || " + clone2 + " || true)");
            if (fs::exists(tmp_clone)) {
                try {
                    fs::copy(tmp_clone, themes_dir + "/anti-snap",
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    Logger::ok(std::string(_("gui.dock.theme_installed")) + themes_dir + "/anti-snap");
                } catch (const fs::filesystem_error &e) {
                    Logger::warn(std::string(_("gui.dock.theme_copy_failed")) + e.what());
                }
                fs::remove_all(tmp_clone, ec);
            }
        } else {
            Logger::info(std::string(_("gui.dock.theme_already_exists")) + themes_dir + "/anti-snap");
        }

        Logger::ok(_("gui.dock.install_done"));
        Logger::info(_("gui.dock.configure_hint"));
        return true;
    }

    bool BeautificationManager::download_wallpaper(std::string_view source) {
        Logger::step(std::string(_("gui.wallpaper.downloading")) + std::string(source));
        std::string wallpaper_dir = "/usr/share/backgrounds/tmoe";
        fs::create_directories(wallpaper_dir);

        std::string url, filename;
        std::string src_lower(source);
        std::transform(src_lower.begin(), src_lower.end(), src_lower.begin(), ::tolower);
        auto fam = get_family(cfg_);

        if (src_lower == "debian" || src_lower == "gnome") {
            PackageManager::install("gnome-backgrounds", fam);
            Logger::ok(_("gui.wallpaper.gnome_installed"));
            return true;
        } else if (src_lower == "xfce" || src_lower == "xubuntu") {
            url = "https://gitlab.xfce.org/artwork/xfce4-artwork/-/raw/master/backgrounds/xfce-stripes.png";
            filename = "xfce-stripes.png";
        } else if (src_lower == "mate" || src_lower == "ubuntu-mate") {
            PackageManager::install("ubuntu-mate-wallpapers", fam);
            Logger::ok(_("gui.wallpaper.mate_installed"));
            return true;
        } else if (src_lower == "deepin") {
            PackageManager::install("deepin-wallpapers", fam);
            Logger::ok(_("gui.wallpaper.deepin_installed"));
            return true;
        } else if (src_lower == "kde") {
            PackageManager::install("plasma-workspace-wallpapers", fam);
            Logger::ok(_("gui.wallpaper.kde_installed"));
            return true;
        } else {
            Logger::info(std::string(_("gui.wallpaper.unknown_source")) + std::string(source) + _("gui.wallpaper.skip"));
            return false;
        }

        if (!url.empty()) {
            SystemHelper::download_file(url, wallpaper_dir + "/" + filename);
            set_wallpaper(wallpaper_dir + "/" + filename);
        }

        Logger::ok(_("gui.wallpaper.download_done"));
        return true;
    }

    bool BeautificationManager::install_conky() {
        Logger::step(_("gui.conky.install_step"));
        if (!PackageManager::install({"conky", "conky-all"}, get_family(cfg_))) {
            Logger::warn(_("gui.conky.install_failed"));
            return false;
        }

        std::string home = SystemHelper::user_home();
        std::string conky_dir = home + "/.config/conky";
        fs::create_directories(conky_dir);

        SystemHelper::write_file(fs::path(conky_dir + "/conky.conf"), R"(conky.config = {
    alignment = 'top_right',
    background = false,
    double_buffer = true,
    font = 'Iosevka:size=10',
    gap_x = 12, gap_y = 48,
    own_window = true,
    own_window_type = 'desktop',
    own_window_transparent = true,
    own_window_argb_visual = true,
    update_interval = 1.0,
    use_xft = true,
};

conky.text = [[
${color #00ff00}Host: ${color white}$nodename
${color #00ff00}Kernel: ${color white}$kernel
${color #00ff00}Uptime: ${color white}$uptime

${color #ff6600}CPU: ${color white}${cpu}%
${cpugraph 20 200}
${color #ff6600}RAM: ${color white}$mem/$memmax
${color #ff6600}Disk: ${color white}${fs_used /}/${fs_size /}
${color #4080ff}Net: ${color white}${addr wlan0} ${addr eth0}
]];
)");

        std::string autostart_dir = home + "/.config/autostart";
        fs::create_directories(autostart_dir);
        SystemHelper::write_file(fs::path(autostart_dir + "/conky.desktop"),
            "[Desktop Entry]\nType=Application\nName=Conky\n"
            "Exec=conky -c " + conky_dir + "/conky.conf\nX-GNOME-Autostart-enabled=true\n");

        Logger::ok(_("gui.conky.install_done"));
        configure_conky();
        return true;
    }

    void BeautificationManager::configure_conky() {
        std::string home = SystemHelper::user_home();
        if (fs::exists(home + "/github/Harmattan")) {
            Logger::info(std::string(_("gui.conky.harmattan_already_exists")) + home + "/github/Harmattan");
            Logger::info(_("gui.conky.harmattan_preview_hint"));
            return;
        }
        fs::create_directories(home + "/github");
        auto clone_cmd = CommandBuilder("git")
            .add_arg("clone").add_flag("--depth=1")
            .add_arg("https://github.com/zagortenay333/Harmattan.git")
            .add_raw("2>/dev/null").build_string();
        auto clone_git_cmd = CommandBuilder("git")
            .add_arg("clone").add_flag("--depth=1")
            .add_arg("git://github.com/zagortenay333/Harmattan.git")
            .add_raw("2>/dev/null").build_string();
        Executor::passthrough("cd '" + home + "/github' && (" + clone_cmd + " || " + clone_git_cmd + " || true)");
        if (fs::exists(home + "/github/Harmattan")) {
            Logger::info(std::string(_("gui.conky.harmattan_downloaded")) + home + "/github/Harmattan");
            Logger::info(_("gui.conky.harmattan_preview_hint"));
        } else {
            Logger::warn(_("gui.conky.harmattan_clone_failed"));
        }
    }

    bool BeautificationManager::install_compiz() {
        Logger::step(_("gui.compiz.install_step"));
        std::vector<std::string> pkgs = {
            "compiz", "compiz-core", "compiz-plugins",
            "compiz-plugins-default", "compiz-plugins-extra",
            "emerald", "emerald-themes", "compizconfig-settings-manager"
        };
        if (!PackageManager::install(pkgs, get_family(cfg_))) {
            Logger::warn(_("gui.compiz.partial_failed"));
            PackageManager::install({"compiz", "compiz-core", "compiz-plugins"}, get_family(cfg_));
        }
        Logger::ok(_("gui.compiz.install_done"));
        Logger::info(_("gui.compiz.emerald_hint"));
        Logger::info(_("gui.compiz.ccsm_hint"));
        return true;
    }

    bool BeautificationManager::install_cursor_theme(std::string_view theme) {
        Logger::step(std::string(_("gui.cursor.install_step")) + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::cursor_theme_package_name(name_lower);
        if (!PackageManager::install(pkg_name, get_family(cfg_))) {
            Logger::warn(_("gui.cursor.install_may_fail"));
            return false;
        }
        Logger::ok(_("gui.cursor.install_done"));
        return true;
    }

    bool BeautificationManager::deploy_xfce_panel_config() {
        Logger::step(_("gui.xfce_panel.deploy_step"));
        std::string home = SystemHelper::user_home();
        fs::path panel_dir = fs::path(home) / ".config" / "xfce4" / "xfconf" / "xfce-perchannel-xml";
        fs::path panel_file = panel_dir / "xfce4-panel.xml";

        if (fs::exists(panel_file)) {
            fs::path backup_dest = fs::path(home) / ".config" / "tmoe-linux" / "xfce4-panel.xml.bak";
            try {
                fs::create_directories(backup_dest.parent_path());
                fs::copy_file(panel_file, backup_dest, fs::copy_options::overwrite_existing);
                Logger::info(std::string(_("gui.xfce_panel.backup_ok")) + backup_dest.string());
            } catch (const fs::filesystem_error &e) {
                Logger::warn(std::string(_("gui.xfce_panel.backup_failed")) + std::string(e.what()));
            }
        }
        fs::create_directories(panel_dir);
        return SystemHelper::write_file(panel_file, gui_config::XFCE_PANEL_XML);
    }

    std::string BeautificationManager::generate_xfce_panel_xml() {
        return gui_config::XFCE_PANEL_XML;
    }

    // ═══════════════════════════════════════════════════════════════
    // 主题菜单
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::configure_theme_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(
            _("gui.theme.menu_title"), _("gui.theme.menu_prompt"), "theme_menu");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.xfce_parser_item"), "1",
            [this](MenuContext&) -> bool {
                xfce_theme_parsing();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.local_installer_item"), "2",
            [this](MenuContext&) -> bool {
                local_theme_installer();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.win10_kali_item"), "3",
            [this](MenuContext&) -> bool {
                desktop_manager_.install_kali_undercover();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.macos_mojave_item"), "4",
            [this](MenuContext&) -> bool {
                desktop_manager_.download_macos_mojave_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.macos_bigsur_item"), "5",
            [this](MenuContext&) -> bool {
                desktop_manager_.download_macos_bigsur_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.breeze_item"), "6",
            [this](MenuContext&) -> bool {
                desktop_manager_.install_breeze_theme_ext();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.kali_flat_remix_item"), "7",
            [this](MenuContext&) -> bool {
                desktop_manager_.download_kali_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.ukui_item"), "8",
            [this](MenuContext&) -> bool {
                PackageManager::install({"ukui-themes", "ukui-greeter"}, get_family(cfg_));
                if (!fs::exists("/usr/share/icons/ukui-icon-theme-default") &&
                    !fs::exists("/usr/share/icons/ukui-icon-theme")) {
                    SystemHelper::fetch_latest_and_extract(
                        "https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/",
                        "ukui-themes.*all\\.deb", "ukui_themes");
                    Executor::shell(
                        "sudo update-icon-caches /usr/share/icons/ukui-icon-theme-basic "
                        "/usr/share/icons/ukui-icon-theme-classical "
                        "/usr/share/icons/ukui-icon-theme-default 2>/dev/null &");
                }
                desktop_manager_.set_default_xfce_icon_theme("ukui-icon-theme");
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.theme.arc_item"), "9",
            [this](MenuContext&) -> bool {
                desktop_manager_.install_arc_gtk_theme_ext();
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    // ═══════════════════════════════════════════════════════════════
    // 主题解析 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::xfce_theme_parsing() {
        std::string url_cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.theme.parser_title"))
                .add_arg("--inputbox").add_arg(_("gui.theme.parser_prompt"))
                .add_arg("0").add_arg("50").build_string();
        std::string url = Executor::tui_select(url_cmd);
        if (url.empty()) return;

        Logger::info(_("gui.theme.parsing_page"));

        // 下载 HTML 页面
        std::string html = SystemHelper::http_get_cached(url, ".theme_index_cache_tmoe.html");
        if (html.empty()) {
            Logger::info(_("gui.theme.no_themes_found"));
            return;
        }

        // 解析主题名：找含 tar.xz|tar.gz 的 title 属性
        std::vector<std::string> theme_names;
        {
            // 将逗号替换为换行以模拟 bash sed 's@,@\n@g'
            std::string flat = std::regex_replace(html, std::regex(","), "\n");
            std::regex title_re(R"re("title"\s*:\s*"([^"]*)")re");
            std::regex ext_re(R"re(\.(tar\.xz|tar\.gz))re");
            auto begin = std::sregex_iterator(flat.begin(), flat.end(), title_re);
            for (auto it = begin; it != std::sregex_iterator(); ++it) {
                std::string t = (*it)[1].str();
                if (std::regex_search(t, ext_re))
                    theme_names.push_back(t);
            }
            std::sort(theme_names.begin(), theme_names.end());
            theme_names.erase(std::unique(theme_names.begin(), theme_names.end()), theme_names.end());
        }

        // 解析下载量
        std::vector<std::string> counts;
        {
            // URL 解码关键字符
            std::string decoded = html;
            decoded = replace_all(decoded, "%2F", "/");
            decoded = replace_all(decoded, "%3A", ":");
            decoded = replace_all(decoded, "%2B", "+");
            decoded = replace_all(decoded, "%3D", "=");
            decoded = replace_all(decoded, "%23", "#");
            decoded = replace_all(decoded, "%26", "&");
            std::string flat = std::regex_replace(decoded, std::regex(","), "\n");
            std::regex dl_re(R"re("downloaded_count"\s*:\s*(\d+))re");
            auto begin = std::sregex_iterator(flat.begin(), flat.end(), dl_re);
            for (auto it = begin; it != std::sregex_iterator(); ++it)
                counts.push_back((*it)[1].str());
        }

        if (theme_names.empty()) {
            Logger::info(_("gui.theme.no_themes_found"));
            return;
        }

        Logger::info(_("gui.theme.found_themes"));
        for (size_t i = 0; i < theme_names.size(); ++i) {
            std::string cnt = i < counts.size() ? counts[i] : "?";
            Logger::info("  " + theme_names[i] + "  " + cnt + _("gui.theme.download_count_suffix"));
        }
        Logger::info(_("gui.theme.manual_select_hint"));
    }

    // ═══════════════════════════════════════════════════════════════
    // 图标主题 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_icon_themes_menu() {
        using namespace tmoe::ui;

        desktop_manager_.check_update_icon_caches_sh();

        auto menu = make_plugin_menu(
            _("gui.icon.menu_title"), _("gui.icon.menu_prompt"), "icon_themes");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.win11_item"), "1",
            [this](MenuContext&) -> bool {
                download_win10x_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.candy_item"), "2",
            [this](MenuContext&) -> bool {
                download_candy_icon_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.pixel_item"), "3",
            [this](MenuContext&) -> bool {
                download_raspbian_pixel_assets();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.paper_item"), "4",
            [this](MenuContext&) -> bool {
                download_paper_icon_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.papirus_item"), "5",
            [this](MenuContext&) -> bool {
                desktop_manager_.download_papirus_icon_theme();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.numix_item"), "6",
            [this](MenuContext&) -> bool {
                desktop_manager_.install_numix_theme_ext();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.moka_item"), "7",
            [this](MenuContext&) -> bool {
                desktop_manager_.install_moka_theme_ext();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.icon.uos_item"), "8",
            [this](MenuContext&) -> bool {
                download_uos_icon_theme();
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::download_win10x_theme() {
        if (fs::exists("/usr/share/icons/We10X-Valley-dark")) {
            Logger::info(_("gui.icon.win10x_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.win10x_download_step"));
        // git clone + tar 解压，tar 保留包内权限
        SystemHelper::git_clone_and_extract("https://gitee.com/mo2/xfce-themes.git", "win10x",
                              "We10X.tar.xz", "/tmp/.WINDOWS_11_ICON_THEME", "/usr/share/icons");
        Executor::shell("sudo update-icon-caches /usr/share/icons/We10X-Valley-dark /usr/share/icons/We10X-Valley 2>/dev/null &");
        desktop_manager_.set_default_xfce_icon_theme("We10X-Valley");
    }

    void BeautificationManager::download_candy_icon_theme() {
        if (fs::exists("/usr/share/icons/candy-icons")) {
            Logger::info(_("gui.icon.candy_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.candy_download_step"));
        std::string tmp_zip = "/tmp/.candy_icons.zip";
        // 双源下载
        if (!SystemHelper::download_file("https://github.com/EliverLara/candy-icons/archive/refs/heads/master.zip", tmp_zip))
            SystemHelper::download_file("https://ghproxy.com/https://github.com/EliverLara/candy-icons/archive/refs/heads/master.zip", tmp_zip);
        if (fs::exists(tmp_zip)) {
            // unzip + mv，unzip 保留 zip 内权限
            Executor::passthrough("cd /tmp && unzip -qo '" + tmp_zip + "' 2>/dev/null && "
                "sudo mv candy-icons-master /usr/share/icons/candy-icons 2>/dev/null || true");
            Executor::shell("sudo update-icon-caches /usr/share/icons/candy-icons 2>/dev/null &");
            std::error_code ec;
            fs::remove(tmp_zip, ec);
        }
        desktop_manager_.set_default_xfce_icon_theme("candy-icons");
    }

    void BeautificationManager::download_uos_icon_theme() {
        PackageManager::install("deepin-icon-theme", get_family(cfg_));
        if (fs::exists("/usr/share/icons/Uos")) {
            Logger::info(_("gui.icon.uos_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.uos_download_step"));
        SystemHelper::git_clone_and_extract("https://gitee.com/mo2/xfce-themes.git", "Uos",
                              "Uos.tar.xz", "/tmp/UosICONS", "/usr/share/icons");
        Executor::shell("sudo update-icon-caches /usr/share/icons/Uos 2>/dev/null &");
        desktop_manager_.set_default_xfce_icon_theme("Uos");
    }

    void BeautificationManager::download_paper_icon_theme() {
        Logger::step(_("gui.icon.paper_download_step"));
        if (SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/",
                                     "paper-icon-theme.*pkg\\.tar", "paper_icons")) {
            Executor::shell("sudo update-icon-caches /usr/share/icons/Paper /usr/share/icons/Paper-Mono-Dark 2>/dev/null &");
        }
        desktop_manager_.set_default_xfce_icon_theme("Paper");
    }

    void BeautificationManager::download_raspbian_pixel_assets() {
        Logger::step(_("gui.pixel.download_step"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/raspberrypi/pool/ui/p/pixel-wallpaper/",
                                 "pixel-wallpaper.*all\\.deb", "pixel_wp");
        desktop_manager_.check_update_icon_caches_sh();
        if (fs::exists("/usr/share/icons/PiX"))
            Executor::shell("sudo update-icon-caches /usr/share/icons/PiX 2>/dev/null &");
        if (fs::exists("/usr/share/icons/raspberrypi"))
            Executor::shell("sudo update-icon-caches /usr/share/icons/raspberrypi 2>/dev/null &");

        std::string home = SystemHelper::user_home();
        if (fs::exists("/usr/share/backgrounds/pixel")) {
            fs::create_directories(SystemHelper::user_pictures_dir());
            std::error_code ec;
            fs::create_symlink("/usr/share/backgrounds/pixel",
                               home + "/Pictures/raspberrypi-pixel-wallpapers", ec);
            Logger::info(_("gui.pixel.wallpaper_linked"));
        }
        Logger::ok(_("gui.pixel.download_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // 壁纸菜单
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_wallpapers_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(
            _("gui.wallpaper.menu_title"), _("gui.wallpaper.menu_prompt"), "wallpapers");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.ubuntu_item"), "1",
            [this](MenuContext&) -> bool {
                ubuntu_wallpapers_and_photos_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.mint_item"), "2",
            [this](MenuContext&) -> bool {
                linux_mint_backgrounds_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.deepin_item"), "3",
            [this](MenuContext&) -> bool {
                download_deepin_wallpaper();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.elementary_item"), "4",
            [this](MenuContext&) -> bool {
                download_elementary_wallpaper();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.raspbian_item"), "5",
            [this](MenuContext&) -> bool {
                download_raspbian_pixel_assets();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.manjaro_item"), "6",
            [this](MenuContext&) -> bool {
                download_manjaro_wallpaper();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.gnome_item"), "7",
            [this](MenuContext&) -> bool {
                download_debian_gnome_wallpaper();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.xfce_artwork_item"), "8",
            [this](MenuContext&) -> bool {
                download_arch_xfce_artwork();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.arch_item"), "9",
            [this](MenuContext&) -> bool {
                download_arch_wallpaper();
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::ubuntu_wallpapers_and_photos_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(
            _("gui.wallpaper.ubuntu_pack_title"), _("gui.wallpaper.ubuntu_pack_prompt"), "ubuntu_wallpapers");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.ubuntu_gnome_item"), "1",
            [this](MenuContext&) -> bool {
                ubuntu_gnome_wallpapers_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.xubuntu_community_item"), "2",
            [this](MenuContext&) -> bool {
                xubuntu_wallpapers_menu();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.ubuntu_mate_item"), "3",
            [this](MenuContext&) -> bool {
                desktop_manager_.download_ubuntu_mate_wallpaper();
                Logger::press_enter();
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.wallpaper.ubuntu_kylin_item"), "4",
            [this](MenuContext&) -> bool {
                download_ubuntu_kylin_wallpaper();
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::ubuntu_gnome_wallpapers_menu() {
        using namespace tmoe::ui;

        const char *codes[] = {
            "artful", "bionic", "cosmic", "disco", "eoan", "focal", "karmic", "lucid", "maverick",
            "natty", "oneiric", "precise", "quantal", "raring", "saucy", "trusty", "utopic", "vivid",
            "wily", "xenial", "yakkety", "zesty", nullptr
        };

        auto menu = make_plugin_menu(
            _("gui.wallpaper.ubuntu_title"), _("gui.wallpaper.ubuntu_prompt"), "ubuntu_gnome_wp");

        for (int i = 0; codes[i]; ++i) {
            std::string code = codes[i];
            std::string tag = std::to_string(i + 1);
            menu->add_child(std::make_shared<LambdaAction>(
                code, tag,
                [this, code](MenuContext&) -> bool {
                    download_ubuntu_wallpaper(code);
                    Logger::press_enter();
                    return true;
                }));
        }

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::xubuntu_wallpapers_menu() {
        using namespace tmoe::ui;

        const char *items[] = {"trusty", "xenial","jammy", "bionic", "focal", nullptr};

        auto menu = make_plugin_menu(
            _("gui.wallpaper.xubuntu_title"), _("gui.wallpaper.xubuntu_prompt"), "xubuntu_wp");

        for (int i = 0; items[i]; ++i) {
            std::string item = items[i];
            std::string tag = std::to_string(i + 1);
            menu->add_child(std::make_shared<LambdaAction>(
                item, tag,
                [this, item](MenuContext&) -> bool {
                    desktop_manager_.download_xubuntu_wallpaper(item);
                    Logger::press_enter();
                    return true;
                }));
        }

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::linux_mint_backgrounds_menu() {
        using namespace tmoe::ui;

        const char *codes[] = {
            "ulyssa", "ulyana", "tricia", "tina", "tessa", "tara", "sylvia", "sonya", "serena",
            "sarah", "rosa", "retro", "rebecca", "rafaela", "qiana", "petra", "olivia", "nadia",
            "maya", "lisa-extra", "katya-extra", "xfce", nullptr
        };

        auto menu = make_plugin_menu(
            _("gui.wallpaper.mint_title"), _("gui.wallpaper.mint_prompt"), "linux_mint_wp");

        for (int i = 0; codes[i]; ++i) {
            std::string code = codes[i];
            std::string tag = std::to_string(i + 1);
            menu->add_child(std::make_shared<LambdaAction>(
                code, tag,
                [this, code](MenuContext&) -> bool {
                    desktop_manager_.download_mint_backgrounds(code);
                    Logger::press_enter();
                    return true;
                }));
        }

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    // ═══════════════════════════════════════════════════════════════
    // 壁纸下载 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_deepin_wallpaper() {
        Logger::step(_("gui.wallpaper.deepin_download"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/deepin/pool/main/d/deepin-wallpapers/",
                                     "deepin-wallpapers_.*tar\\.xz", "deepin_wp");
    }

    void BeautificationManager::download_elementary_wallpaper() {
        Logger::step(_("gui.wallpaper.elementary_download"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/archlinux/extra/os/x86_64/",
                                 "elementary-wallpapers.*tar\\.zst$", "elementary_wp");
    }

    void BeautificationManager::download_arch_wallpaper() {
        link_to_debian_wallpaper();
        Logger::step(_("gui.wallpaper.arch_download"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/archlinux/extra/os/x86_64/",
                                 "archlinux-wallpaper.*tar\\.zst$", "arch_wp");
    }

    void BeautificationManager::download_manjaro_wallpaper() {
        Logger::step(_("gui.wallpaper.manjaro_download"));
        // Manjaro 壁纸是固定名称的 pkg.tar.xz，直接下载
        std::string tmp1 = "/tmp/.manjaro_wp2018.deb";// zst似乎是和deb有关系来着，反正走deb解析就对了
        // std::string tmp2 = "/tmp/.manjaro_wp2017.tar.xz";
        SystemHelper::download_file("https://mirrors.tuna.tsinghua.edu.cn/manjaro/pool/overlay/wallpapers-2018-1.2-2-any.pkg.tar.zst", tmp1);
        if (fs::exists(tmp1)) { SystemHelper::extract_archive(tmp1); std::error_code ec; fs::remove(tmp1, ec); }
        // 找不到下载地址，暂时注释
        // SystemHelper::download_file("https://mirrors.tuna.tsinghua.edu.cn/manjaro/pool/overlay/manjaro-sx-wallpapers-20171023-1-any.pkg.tar.xz", tmp2);
        // if (fs::exists(tmp2)) { SystemHelper::extract_archive(tmp2); std::error_code ec; fs::remove(tmp2, ec); }
    }

    void BeautificationManager::download_arch_xfce_artwork() {
        Logger::step(_("gui.wallpaper.arch_xfce_artwork"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.tuna.tsinghua.edu.cn/ubuntu/pool/universe/x/xfce4-artwork/",
                                 "xfce4-artwork.*all\\.deb", "xfce_art");
    }

    void BeautificationManager::download_debian_gnome_wallpaper() {
        Logger::step(_("gui.wallpaper.debian_gnome_download"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/debian/pool/main/g/gnome-backgrounds/",
                                 "gnome-backgrounds.*all\\.deb", "gnome_bg");
    }

    void BeautificationManager::download_ubuntu_kylin_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_kylin_download"));
        SystemHelper::fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntukylin-wallpapers/",
                                 "ubuntukylin-wallpapers_.*\\.tar\\.xz", "ukylin_wp");
    }

    void BeautificationManager::download_ubuntu_wallpaper(const std::string &ubuntu_code) {
        Logger::step(std::string(_("gui.wallpaper.ubuntu_download")) + ubuntu_code);
        std::string home = SystemHelper::user_home();
        fs::path wp_dir = fs::path(home) / "Pictures" / "ubuntu-wallpapers";
        fs::create_directories(wp_dir);

        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-wallpapers/";
        std::string pattern = "ubuntu-wallpapers-" + ubuntu_code + ".*all\\.deb";
        std::string html = SystemHelper::http_get_cached(repo, ".ubuntu_wp_index.html");
        if (html.empty()) return;

        std::string pkg_name = SystemHelper::find_latest_href(html, pattern);
        if (pkg_name.empty()) return;

        std::string pkg_path = wp_dir.string() + "/ubuntu-wallpaper.deb";
        if (SystemHelper::download_file(repo + pkg_name, pkg_path)) {
            SystemHelper::extract_archive(pkg_path, wp_dir.string());
            std::error_code ec;
            fs::remove(pkg_path, ec);
            fs::remove(wp_dir.string() + "/data.tar.xz", ec);
        }
    }

    void BeautificationManager::link_to_debian_wallpaper() {
        std::string home = SystemHelper::user_home();
        fs::create_directories(SystemHelper::user_pictures_dir());
        std::error_code ec;
        if (fs::exists("/usr/share/backgrounds/kali/"))
            fs::create_symlink("/usr/share/backgrounds/kali/", home + "/Pictures/kali", ec);
        if (fs::exists("/usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/"))
            fs::create_symlink("/usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/",
                               home + "/Pictures/debian-moonlight", ec);
    }

    void BeautificationManager::check_theme_url(std::string &url) {
        if (contains(url, "www") && !contains(url, "http"))
            url = "https://" + url;
    }

    // ═══════════════════════════════════════════════════════════════
    // 光标主题 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::configure_mouse_cursor() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(
            _("gui.cursor.menu_title"), _("gui.cursor.menu_prompt"), "mouse_cursor");

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.cursor.all_modern_item"), "1",
            [this](MenuContext&) -> bool {
                download_chameleon_cursor_theme();
                Logger::info(_("gui.cursor.apply_hint"));
                Logger::press_enter();
                return true;
            }));

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.cursor.breeze_item"), "2",
            [this](MenuContext&) -> bool {
                Logger::step(std::string(_("gui.cursor.install_step")) + "breeze-cursor-theme");
                std::string name = "breeze-cursor-theme";
                std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/";
                std::string pattern = name + ".*all\\.deb";
                if (!SystemHelper::fetch_latest_and_extract(repo, pattern, name + "_dl")) {
                    std::string fallback = repo;
                    size_t p = fallback.find("mirrors.bfsu.edu.cn");
                    if (p != std::string::npos) fallback.replace(p, 21, "ftp.debian.org");
                    SystemHelper::fetch_latest_and_extract(fallback, pattern, name + "_dl_fb");
                }
                Logger::ok(_("gui.cursor.install_done"));
                Logger::info(_("gui.cursor.apply_hint"));
                Logger::press_enter();
                return true;
            }));

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.cursor.chameleon_item"), "3",
            [this](MenuContext&) -> bool {
                Logger::step(std::string(_("gui.cursor.install_step")) + "chameleon-cursor-theme");
                std::string name = "chameleon-cursor-theme";
                std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/";
                std::string pattern = name + ".*all\\.deb";
                if (!SystemHelper::fetch_latest_and_extract(repo, pattern, name + "_dl")) {
                    std::string fallback = repo;
                    size_t p = fallback.find("mirrors.bfsu.edu.cn");
                    if (p != std::string::npos) fallback.replace(p, 21, "ftp.debian.org");
                    SystemHelper::fetch_latest_and_extract(fallback, pattern, name + "_dl_fb");
                }
                Logger::ok(_("gui.cursor.install_done"));
                Logger::info(_("gui.cursor.apply_hint"));
                Logger::press_enter();
                return true;
            }));

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.cursor.moblin_item"), "4",
            [this](MenuContext&) -> bool {
                Logger::step(std::string(_("gui.cursor.install_step")) + "moblin-cursor-theme");
                std::string name = "moblin-cursor-theme";
                std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/";
                std::string pattern = name + ".*all\\.deb";
                if (!SystemHelper::fetch_latest_and_extract(repo, pattern, name + "_dl")) {
                    std::string fallback = repo;
                    size_t p = fallback.find("mirrors.bfsu.edu.cn");
                    if (p != std::string::npos) fallback.replace(p, 21, "ftp.debian.org");
                    SystemHelper::fetch_latest_and_extract(fallback, pattern, name + "_dl_fb");
                }
                Logger::ok(_("gui.cursor.install_done"));
                Logger::info(_("gui.cursor.apply_hint"));
                Logger::press_enter();
                return true;
            }));

        menu->add_child(std::make_shared<LambdaAction>(
            _("gui.cursor.install_via_pkg_item"), "5",
            [this](MenuContext&) -> bool {
                std::string pkg_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.cursor.pkg_install_title"))
                        .add_arg("--inputbox").add_arg(_("gui.cursor.pkg_install_prompt"))
                        .add_arg("0").add_arg("50").add_arg("breeze-cursor-theme")
                        .build_string();
                std::string pkg_name = Executor::tui_select(pkg_cmd);
                if (!pkg_name.empty()) install_cursor_theme(pkg_name);
                Logger::info(_("gui.cursor.apply_hint"));
                Logger::press_enter();
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void BeautificationManager::download_chameleon_cursor_theme() {
        const std::pair<const char *, const char *> cursors[] = {
            {"breeze-cursor-theme",    "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/"},
            {"chameleon-cursor-theme", "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/"},
            {"moblin-cursor-theme",    "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/"},
        };
        for (const auto &c : cursors) {
            SystemHelper::fetch_latest_and_extract(c.second,
                std::string(c.first) + ".*all\\.deb",
                std::string(c.first) + "_dl");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 内置工具
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::local_theme_installer() {
        Logger::info(_("gui.theme.local_installer"));
        Logger::info(_("gui.theme.place_in_tmp"));

        std::string cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.theme.local_installer_title"))
                .add_arg("--inputbox").add_arg(_("gui.theme.local_installer_prompt"))
                .add_arg("10").add_arg("60").build_string();
        std::string filename = Executor::tui_select(cmd);
        if (filename.empty()) return;

        std::string full_path = "/tmp/" + filename;
        if (!fs::exists(full_path)) {
            Logger::error(std::string(_("gui.theme.file_not_found")) + full_path);
            return;
        }

        auto type_r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--title").add_arg(_("gui.theme.file_type_title"))
            .add_arg("--yes-button").add_arg(_("gui.theme.type_theme"))
            .add_arg("--no-button").add_arg(_("gui.theme.type_icon"))
            .add_arg("--yesno").add_arg(_("gui.theme.file_type_prompt"))
            .add_arg("0").add_arg("50").build_string());
        bool is_icon = (type_r.exit_code == 1);
        tmoe_theme_installer(full_path, is_icon);
    }

    void BeautificationManager::tmoe_theme_installer(const std::string &file_path, bool is_icon) {
        std::string extract_path = is_icon ? "/usr/share/icons" : "/usr/share/themes";
        fs::create_directories(extract_path);
        // tar -xf 保留压缩包内原始权限
        auto tar_cmd = CommandBuilder("tar").add_arg("-xf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        auto tar_j_cmd = CommandBuilder("tar").add_arg("-Jxf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        Executor::passthrough(tar_cmd + " 2>/dev/null || " + tar_j_cmd + " 2>/dev/null || true");
        Logger::ok(std::string(_("gui.theme.installed_to")) + extract_path);
    }

    void BeautificationManager::download_and_cat_icon_img(const std::string &url, const std::string &filename) {
        std::string path = "/tmp/" + filename;
        if (!fs::exists(path))
            SystemHelper::download_file(url, path);
        if (Executor::has("catimg") && fs::exists(path))
            Executor::passthrough(CommandBuilder("catimg").add_arg(path).build_string() + " 2>/dev/null || true");
    }

    void BeautificationManager::catimg_preview_lxde_mate_xfce() {
        std::string lxde_url = cfg_.is_wsl
                ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/BUSYeSLZRqq3i3oM.png"
                : "https://gitee.com/ak2/icons/raw/master/raspbian-lxde.jpg";
        std::string mate_url = cfg_.is_wsl
                ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/1frRp1lpOXLPz6mO.jpg"
                : "https://gitee.com/ak2/icons/raw/master/ubuntu-mate.jpg";
        std::string xfce_url = cfg_.is_wsl
                ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/a7IQ9NnfgPckuqRt.jpg"
                : "https://gitee.com/ak2/icons/raw/master/debian-xfce.jpg";

        download_and_cat_icon_img(lxde_url, "LXDE_BUSYeSLZRqq3i3oM.png");
        download_and_cat_icon_img(mate_url, "MATE_1frRp1lpOXLPz6mO.jpg");
        download_and_cat_icon_img(xfce_url, "XFCE_a7IQ9NnfgPckuqRt.jpg");

        if (cfg_.is_wsl) {
            fs::create_directories("/mnt/c/Users/Public/Downloads/VcXsrv");
            std::error_code ec;
            fs::copy_file("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg",
                          "/mnt/c/Users/Public/Downloads/VcXsrv/XFCE_a7IQ9NnfgPckuqRt.jpg",
                          fs::copy_options::overwrite_existing, ec);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Manjaro 包下载（通用工具）
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_manjaro_pkg(const std::string &theme_name,
                                                     const std::string &url,
                                                     const std::string &url_02,
                                                     const std::string & /*wallpaper_name*/,
                                                     const std::string & /*custom_name*/) {
        // 下载 pkg.tar.xz 到 /tmp，解压到 /
        std::string tmp = "/tmp/." + theme_name + "_pkg.tar.xz";
        if (!SystemHelper::download_file(url, tmp))
            SystemHelper::download_file(url_02, tmp);
        if (fs::exists(tmp)) {
            SystemHelper::extract_archive(tmp);
            std::error_code ec;
            fs::remove(tmp, ec);
        }
    }

} // namespace tmoe::domain
