#include "beautification_manager.h"
#include "desktop_manager.h"
#include "../gui_config/templates.h"
#include "domain/system/package_manager.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "core/command_builder.hpp"
#include <filesystem>
#include <regex>
#include <sstream>
#include <cstdlib>
#include <cstring>

namespace fs = std::filesystem;

namespace tmoe::domain {
    namespace {
        DistroFamily get_family(const TmoeConfig &cfg) {
            auto f = infer_family_from_config(cfg.linux_distro);
            if (f == DistroFamily::Unknown)
                f = PackageManager::detect_distro_family();
            return f;
        }

        // ═══════════════════════════════════════════════════════════════
        // 原生 C++ 下载/解压 helpers — 消除 bash 管道，保障权限安全
        // ═══════════════════════════════════════════════════════════════

        /// 用 curl/wget 下载 URL，返回内容字符串（缓存到 /tmp）
        std::string http_get_cached(const std::string &url, const std::string &cache_name) {
            fs::path cache_path = fs::path("/tmp") / cache_name;
            // 用单条命令下载（无管道），curl 优先 → wget 回退
            std::string dl_cmd = CommandBuilder("curl").add_arg("-sL").add_arg(url)
                    .add_arg("-o").add_arg(cache_path.string()).build_string()
                    + " 2>/dev/null || " +
                    CommandBuilder("wget").add_arg("-qO").add_arg(cache_path.string()).add_arg(url)
                    .build_string() + " 2>/dev/null || true";
            Executor::passthrough(dl_cmd);
            return SystemHelper::read_file(cache_path);
        }

        /// 从 HTML 索引页中查找匹配正则的最新包文件名
        std::string find_latest_href(const std::string &html, const std::string &pkg_pattern) {
            std::regex href_re(R"re(<a\s+[^>]*href\s*=\s*"([^"]*)"[^>]*>)re", std::regex::icase);
            std::regex pkg_re(pkg_pattern);
            std::string best;
            auto begin = std::sregex_iterator(html.begin(), html.end(), href_re);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                std::string href = (*it)[1].str();
                if (std::regex_search(href, pkg_re)) {
                    best = href;  // 最后匹配的通常是最高版本
                }
            }
            return best;
        }

        /// 下载单个文件（curl/wget/aria2c 回退链），成功返 true
        bool download_file(const std::string &url, const std::string &dest_path) {
            std::string cmd =
                "(curl -sL '" + url + "' -o '" + dest_path + "' 2>/dev/null || "
                "wget -qO '" + dest_path + "' '" + url + "' 2>/dev/null || "
                "aria2c -q --no-conf --allow-overwrite=true -o '" + dest_path + "' '" + url + "' 2>/dev/null)";
            return Executor::passthrough(cmd).ok() && fs::exists(dest_path);
        }

        /// 解压归档到目标目录（tar 保留原始权限）
        void extract_archive(const std::string &archive_path, const std::string &dest = "/") {
            // tar 自动检测格式并保留权限；.deb 需先用 ar 提取 data.tar.xz
            std::string cmd =
                "cd '" + dest + "' && (tar -xf '" + archive_path + "' 2>/dev/null || "
                "(ar x '" + archive_path + "' data.tar.xz 2>/dev/null && "
                "tar -Jxf data.tar.xz 2>/dev/null && rm -f data.tar.xz) || "
                "unzip -qo '" + archive_path + "' -d '" + dest + "' 2>/dev/null || true)";
            Executor::passthrough(cmd);
        }

        /// 通用方法：从仓库索引下载最新包并解压
        /// @param repo_url   镜像仓库 URL (e.g. "https://mirrors.../deepin-wallpapers/")
        /// @param pkg_pattern 包名正则 (e.g. "deepin-community-wallpapers.*all\\.deb")
        /// @param tmp_prefix  临时文件名前缀 (e.g. "deepin_wp")
        /// @param extract_to  解压目标目录，默认 /
        /// @return true 如果成功下载并解压
        bool fetch_latest_and_extract(const std::string &repo_url,
                                       const std::string &pkg_pattern,
                                       const std::string &tmp_prefix,
                                       const std::string &extract_to = "/") {
            // 1. 下载 HTML 索引
            std::string html = http_get_cached(repo_url, "." + tmp_prefix + "_index.html");
            if (html.empty()) return false;

            // 2. 正则解析最新包名
            std::string pkg_name = find_latest_href(html, pkg_pattern);
            if (pkg_name.empty()) return false;

            // 3. 下载包文件
            std::string pkg_url = repo_url + pkg_name;
            std::string pkg_path = "/tmp/." + tmp_prefix + "_pkg";
            if (!download_file(pkg_url, pkg_path)) return false;

            // 4. 解压（tar 保留包内原始权限）
            extract_archive(pkg_path, extract_to);

            // 5. 清理临时文件
            std::error_code ec;
            fs::remove(pkg_path, ec);
            fs::remove("/tmp/." + tmp_prefix + "_index.html", ec);
            fs::remove("/tmp/data.tar.xz", ec);
            return true;
        }

        /// git clone 浅克隆 + tar 解压到目标目录
        bool git_clone_and_extract(const std::string &git_url,
                                    const std::string &branch,
                                    const std::string &archive_name,
                                    const std::string &tmp_dir,
                                    const std::string &extract_to) {
            std::error_code ec;
            fs::remove_all(tmp_dir, ec);
            fs::create_directories(tmp_dir, ec);
            std::string cmd = "cd '" + tmp_dir + "' && "
                "git clone -b '" + branch + "' --depth=1 '" + git_url + "' repo 2>/dev/null && "
                "tar -Jxf repo/" + archive_name + " -C '" + extract_to + "' 2>/dev/null";
            bool ok = Executor::passthrough(cmd).ok();
            fs::remove_all(tmp_dir, ec);
            return ok;
        }

        /// 获取 HOME 目录
        std::string user_home() {
            const char *h = std::getenv("HOME");
            return h ? std::string(h) : "/root";
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
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.beautify_title"))
                    .add_arg("--menu").add_arg(_("gui.beautify_prompt"))
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("1").add_arg(_("gui.beautify.themes"))
                    .add_arg("2").add_arg(_("gui.beautify.icon_theme"))
                    .add_arg("3").add_arg(_("gui.beautify.wallpaper"))
                    .add_arg("4").add_arg(_("gui.beautify.mouse_cursor"))
                    .add_arg("5").add_arg(_("gui.beautify.dock"))
                    .add_arg("6").add_arg(_("gui.beautify.compiz"))
                    .add_arg("0").add_arg(_("menu.tui.back_upper"))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                configure_theme_menu();
            } else if (choice == "2") {
                download_icon_themes_menu();
            } else if (choice == "3") {
                download_wallpapers_menu();
            } else if (choice == "4") {
                configure_mouse_cursor();
            } else if (choice == "5") {
                install_dock();
            } else if (choice == "6") {
                install_compiz();
            }
            Logger::press_enter();
        }
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
            download_file("https://gitee.com/mo2/xfce-themes/raw/terminal/colorschemes.tar.xz", tmp_arc);
            if (fs::exists(tmp_arc)) {
                // tar -Jxf 保留原始权限
                Executor::passthrough("cd '" + colorscheme_dir + "' && tar -Jxf '" + tmp_arc + "' 2>/dev/null || true");
                std::error_code ec;
                fs::remove(tmp_arc, ec);
            }
        }

        // 2. 创建 ~/.config/xfce4/terminal/terminalrc（使用统一模板）
        std::string home = user_home();
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

        std::string home = user_home();
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
            Executor::passthrough("cd /tmp && (git clone --depth=1 https://github.com/TaylanTatli/Anti-Snap.git "
                + tmp_clone + " 2>/dev/null || git clone --depth=1 git://github.com/TaylanTatli/Anti-Snap.git "
                + tmp_clone + " 2>/dev/null || true)");
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
            download_file(url, wallpaper_dir + "/" + filename);
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

        std::string home = user_home();
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
        std::string home = user_home();
        if (fs::exists(home + "/github/Harmattan")) {
            Logger::info(std::string(_("gui.conky.harmattan_already_exists")) + home + "/github/Harmattan");
            Logger::info(_("gui.conky.harmattan_preview_hint"));
            return;
        }
        fs::create_directories(home + "/github");
        Executor::passthrough("cd '" + home + "/github' && "
            "(git clone --depth=1 https://github.com/zagortenay333/Harmattan.git 2>/dev/null || "
            "git clone --depth=1 git://github.com/zagortenay333/Harmattan.git 2>/dev/null || true)");
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
        std::string home = user_home();
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
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.theme.menu_title"))
                    .add_arg("--menu").add_arg(_("gui.theme.menu_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.theme.xfce_parser_item"))
                    .add_arg("2").add_arg(_("gui.theme.local_installer_item"))
                    .add_arg("3").add_arg(_("gui.theme.win10_kali_item"))
                    .add_arg("4").add_arg(_("gui.theme.macos_mojave_item"))
                    .add_arg("5").add_arg(_("gui.theme.macos_bigsur_item"))
                    .add_arg("6").add_arg(_("gui.theme.breeze_item"))
                    .add_arg("7").add_arg(_("gui.theme.kali_flat_remix_item"))
                    .add_arg("8").add_arg(_("gui.theme.ukui_item"))
                    .add_arg("9").add_arg(_("gui.theme.arc_item"))
                    .add_arg("0").add_arg(_("gui.theme.return_prev"))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") xfce_theme_parsing();
            else if (ch == "2") local_theme_installer();
            else if (ch == "3") desktop_manager_.install_kali_undercover();
            else if (ch == "4") desktop_manager_.download_macos_mojave_theme();
            else if (ch == "5") desktop_manager_.download_macos_bigsur_theme();
            else if (ch == "6") desktop_manager_.install_breeze_theme_ext();
            else if (ch == "7") desktop_manager_.download_kali_theme();
            else if (ch == "8") {
                PackageManager::install({"ukui-themes", "ukui-greeter"}, get_family(cfg_));
                if (!fs::exists("/usr/share/icons/ukui-icon-theme-default") &&
                    !fs::exists("/usr/share/icons/ukui-icon-theme")) {
                    fetch_latest_and_extract(
                        "https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/",
                        "ukui-themes.*all\\.deb", "ukui_themes");
                    Executor::shell(
                        "update-icon-caches /usr/share/icons/ukui-icon-theme-basic "
                        "/usr/share/icons/ukui-icon-theme-classical "
                        "/usr/share/icons/ukui-icon-theme-default 2>/dev/null &");
                }
                desktop_manager_.set_default_xfce_icon_theme("ukui-icon-theme");
            } else if (ch == "9") desktop_manager_.install_arc_gtk_theme_ext();
            Logger::press_enter();
        }
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
        std::string html = http_get_cached(url, ".theme_index_cache_tmoe.html");
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
            for (const auto &p : {std::make_pair("%2F", "/"), std::make_pair("%3A", ":"),
                                   std::make_pair("%2B", "+"), std::make_pair("%3D", "="),
                                   std::make_pair("%23", "#"), std::make_pair("%26", "&")}) {
                size_t pos = 0;
                while ((pos = decoded.find(p.first, pos)) != std::string::npos) {
                    decoded.replace(pos, 3, p.second);
                    pos += std::strlen(p.second);
                }
            }
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
        while (true) {
            desktop_manager_.check_update_icon_caches_sh();
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.icon.menu_title"))
                    .add_arg("--menu").add_arg(_("gui.icon.menu_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.icon.win11_item"))
                    .add_arg("2").add_arg(_("gui.icon.candy_item"))
                    .add_arg("3").add_arg(_("gui.icon.pixel_item"))
                    .add_arg("4").add_arg(_("gui.icon.paper_item"))
                    .add_arg("5").add_arg(_("gui.icon.papirus_item"))
                    .add_arg("6").add_arg(_("gui.icon.numix_item"))
                    .add_arg("7").add_arg(_("gui.icon.moka_item"))
                    .add_arg("8").add_arg(_("gui.icon.uos_item"))
                    .add_arg("0").add_arg(_("gui.icon.return_prev"))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") download_win10x_theme();
            else if (ch == "2") download_candy_icon_theme();
            else if (ch == "3") download_raspbian_pixel_assets();
            else if (ch == "4") download_paper_icon_theme();
            else if (ch == "5") desktop_manager_.download_papirus_icon_theme();
            else if (ch == "6") desktop_manager_.install_numix_theme_ext();
            else if (ch == "7") desktop_manager_.install_moka_theme_ext();
            else if (ch == "8") download_uos_icon_theme();
            Logger::press_enter();
        }
    }

    void BeautificationManager::download_win10x_theme() {
        if (fs::exists("/usr/share/icons/We10X-Valley-dark")) {
            Logger::info(_("gui.icon.win10x_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.win10x_download_step"));
        // git clone + tar 解压，tar 保留包内权限
        git_clone_and_extract("https://gitee.com/mo2/xfce-themes.git", "win10x",
                              "We10X.tar.xz", "/tmp/.WINDOWS_11_ICON_THEME", "/usr/share/icons");
        Executor::shell("update-icon-caches /usr/share/icons/We10X-Valley-dark /usr/share/icons/We10X-Valley 2>/dev/null &");
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
        if (!download_file("https://github.com/EliverLara/candy-icons/archive/refs/heads/master.zip", tmp_zip))
            download_file("https://ghproxy.com/https://github.com/EliverLara/candy-icons/archive/refs/heads/master.zip", tmp_zip);
        if (fs::exists(tmp_zip)) {
            // unzip + mv，unzip 保留 zip 内权限
            Executor::passthrough("cd /tmp && unzip -qo '" + tmp_zip + "' 2>/dev/null && "
                "mv candy-icons-master /usr/share/icons/candy-icons 2>/dev/null || true");
            Executor::shell("update-icon-caches /usr/share/icons/candy-icons 2>/dev/null &");
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
        git_clone_and_extract("https://gitee.com/mo2/xfce-themes.git", "Uos",
                              "Uos.tar.xz", "/tmp/UosICONS", "/usr/share/icons");
        Executor::shell("update-icon-caches /usr/share/icons/Uos 2>/dev/null &");
        desktop_manager_.set_default_xfce_icon_theme("Uos");
    }

    void BeautificationManager::download_paper_icon_theme() {
        Logger::step(_("gui.icon.paper_download_step"));
        if (fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/",
                                     "paper-icon-theme.*pkg\\.tar", "paper_icons")) {
            Executor::shell("update-icon-caches /usr/share/icons/Paper /usr/share/icons/Paper-Mono-Dark 2>/dev/null &");
        }
        desktop_manager_.set_default_xfce_icon_theme("Paper");
    }

    void BeautificationManager::download_raspbian_pixel_assets() {
        Logger::step(_("gui.pixel.download_step"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/raspberrypi/pool/ui/p/pixel-wallpaper/",
                                 "pixel-wallpaper.*all\\.deb", "pixel_wp");
        desktop_manager_.check_update_icon_caches_sh();
        if (fs::exists("/usr/share/icons/PiX"))
            Executor::shell("update-icon-caches /usr/share/icons/PiX 2>/dev/null &");
        if (fs::exists("/usr/share/icons/raspberrypi"))
            Executor::shell("update-icon-caches /usr/share/icons/raspberrypi 2>/dev/null &");

        std::string home = user_home();
        if (fs::exists("/usr/share/backgrounds/pixel")) {
            fs::create_directories(home + "/Pictures");
            Executor::shell("ln -sf /usr/share/backgrounds/pixel " + home +
                            "/Pictures/raspberrypi-pixel-wallpapers 2>/dev/null || true");
            Logger::info(_("gui.pixel.wallpaper_linked"));
        }
        Logger::ok(_("gui.pixel.download_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // 壁纸菜单
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_wallpapers_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.wallpaper.menu_title"))
                    .add_arg("--menu").add_arg(_("gui.wallpaper.menu_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.wallpaper.ubuntu_item"))
                    .add_arg("2").add_arg(_("gui.wallpaper.mint_item"))
                    .add_arg("3").add_arg(_("gui.wallpaper.deepin_item"))
                    .add_arg("4").add_arg(_("gui.wallpaper.elementary_item"))
                    .add_arg("5").add_arg(_("gui.wallpaper.raspbian_item"))
                    .add_arg("6").add_arg(_("gui.wallpaper.manjaro_item"))
                    .add_arg("7").add_arg(_("gui.wallpaper.gnome_item"))
                    .add_arg("8").add_arg(_("gui.wallpaper.xfce_artwork_item"))
                    .add_arg("9").add_arg(_("gui.wallpaper.arch_item"))
                    .add_arg("0").add_arg(_("gui.wallpaper.return_prev"))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") ubuntu_wallpapers_and_photos_menu();
            else if (ch == "2") linux_mint_backgrounds_menu();
            else if (ch == "3") download_deepin_wallpaper();
            else if (ch == "4") download_elementary_wallpaper();
            else if (ch == "5") download_raspbian_pixel_assets();
            else if (ch == "6") download_manjaro_wallpaper();
            else if (ch == "7") download_debian_gnome_wallpaper();
            else if (ch == "8") download_arch_xfce_artwork();
            else if (ch == "9") download_arch_wallpaper();
            Logger::press_enter();
        }
    }

    void BeautificationManager::ubuntu_wallpapers_and_photos_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.wallpaper.ubuntu_pack_title"))
                    .add_arg("--menu").add_arg(_("gui.wallpaper.ubuntu_pack_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.wallpaper.ubuntu_gnome_item"))
                    .add_arg("2").add_arg(_("gui.wallpaper.xubuntu_community_item"))
                    .add_arg("3").add_arg(_("gui.wallpaper.ubuntu_mate_item"))
                    .add_arg("4").add_arg(_("gui.wallpaper.ubuntu_kylin_item"))
                    .add_arg("0").add_arg(_("menu.tui.back"))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") ubuntu_gnome_wallpapers_menu();
            else if (ch == "2") xubuntu_wallpapers_menu();
            else if (ch == "3") desktop_manager_.download_ubuntu_mate_wallpaper();
            else if (ch == "4") download_ubuntu_kylin_wallpaper();
        }
    }

    void BeautificationManager::ubuntu_gnome_wallpapers_menu() {
        const char *codes[] = {
            "artful", "bionic", "cosmic", "disco", "eoan", "focal", "karmic", "lucid", "maverick",
            "natty", "oneiric", "precise", "quantal", "raring", "saucy", "trusty", "utopic", "vivid",
            "wily", "xenial", "yakkety", "zesty", nullptr
        };
        CommandBuilder menu(cfg_.tui_bin);
        menu.add_arg("--title").add_arg(_("gui.wallpaper.ubuntu_title"))
                .add_arg("--menu").add_arg(_("gui.wallpaper.ubuntu_prompt"))
                .add_arg("0").add_arg("50").add_arg("0");
        for (int i = 0; codes[i]; ++i)
            menu.add_arg(std::to_string(i + 1)).add_arg(codes[i]);
        menu.add_arg("0").add_arg(_("menu.tui.back"));
        auto ch = Executor::tui_select(menu.build_string());
        if (ch == "0" || ch.empty()) return;
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < 22) download_ubuntu_wallpaper(codes[idx]);
    }

    void BeautificationManager::xubuntu_wallpapers_menu() {
        const char *items[] = {"xubuntu-trusty", "xubuntu-xenial", "xubuntu-bionic", "xubuntu-focal", nullptr};
        const char *folders[] = {
            "xubuntu-community-artwork/trusty", "xubuntu-community-artwork/xenial",
            "xubuntu-community-artwork/bionic", "xubuntu-community-artwork/focal"
        };
        CommandBuilder menu(cfg_.tui_bin);
        menu.add_arg("--title").add_arg(_("gui.wallpaper.xubuntu_title"))
                .add_arg("--menu").add_arg(_("gui.wallpaper.xubuntu_prompt"))
                .add_arg("0").add_arg("50").add_arg("0");
        for (int i = 0; items[i]; ++i)
            menu.add_arg(std::to_string(i + 1)).add_arg(items[i]);
        menu.add_arg("0").add_arg(_("menu.tui.back"));
        auto ch = Executor::tui_select(menu.build_string());
        if (ch == "0" || ch.empty()) return;
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < 4) desktop_manager_.download_xubuntu_wallpaper(items[idx], folders[idx]);
    }

    void BeautificationManager::linux_mint_backgrounds_menu() {
        const char *codes[] = {
            "ulyssa", "ulyana", "tricia", "tina", "tessa", "tara", "sylvia", "sonya", "serena",
            "sarah", "rosa", "retro", "rebecca", "rafaela", "qiana", "petra", "olivia", "nadia",
            "maya", "lisa-extra", "katya-extra", "xfce", nullptr
        };
        while (true) {
            CommandBuilder menu(cfg_.tui_bin);
            menu.add_arg("--title").add_arg(_("gui.wallpaper.mint_title"))
                    .add_arg("--menu").add_arg(_("gui.wallpaper.mint_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0");
            for (int i = 0; codes[i]; ++i)
                menu.add_arg(std::to_string(i + 1)).add_arg(codes[i]);
            menu.add_arg("0").add_arg(_("menu.tui.back"));
            auto ch = Executor::tui_select(menu.build_string());
            if (ch == "0" || ch.empty()) return;
            int idx = std::stoi(ch) - 1;
            if (idx >= 0 && idx < 22) {
                desktop_manager_.download_mint_backgrounds(codes[idx]);
                Logger::press_enter();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 壁纸下载 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::download_deepin_wallpaper() {
        Logger::step(_("gui.wallpaper.deepin_download"));
        // 尝试两个包名模式
        if (!fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/deepin/pool/main/d/deepin-wallpapers/",
                                      "deepin-community-wallpapers.*all\\.deb", "deepin_wp"))
            fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/deepin/pool/main/d/deepin-wallpapers/",
                                     "deepin-wallpapers_.*all\\.deb", "deepin_wp2");
    }

    void BeautificationManager::download_elementary_wallpaper() {
        Logger::step(_("gui.wallpaper.elementary_download"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/archlinux/pool/community/",
                                 "elementary-wallpapers.*pkg\\.tar", "elementary_wp");
    }

    void BeautificationManager::download_arch_wallpaper() {
        link_to_debian_wallpaper();
        Logger::step(_("gui.wallpaper.arch_download"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/archlinux/pool/community/",
                                 "archlinux-wallpaper.*pkg\\.tar", "arch_wp");
    }

    void BeautificationManager::download_manjaro_wallpaper() {
        Logger::step(_("gui.wallpaper.manjaro_download"));
        // Manjaro 壁纸是固定名称的 pkg.tar.xz，直接下载
        std::string tmp1 = "/tmp/.manjaro_wp2018.tar.xz";
        std::string tmp2 = "/tmp/.manjaro_wp2017.tar.xz";
        download_file("https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/wallpapers-2018-1.2-1-any.pkg.tar.xz", tmp1);
        if (fs::exists(tmp1)) { extract_archive(tmp1); std::error_code ec; fs::remove(tmp1, ec); }
        download_file("https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/manjaro-sx-wallpapers-20171023-1-any.pkg.tar.xz", tmp2);
        if (fs::exists(tmp2)) { extract_archive(tmp2); std::error_code ec; fs::remove(tmp2, ec); }
    }

    void BeautificationManager::download_arch_xfce_artwork() {
        Logger::step(_("gui.wallpaper.arch_xfce_artwork"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/archlinux/extra/os/x86_64/",
                                 "xfce4-artwork.*pkg\\.tar", "xfce_art");
    }

    void BeautificationManager::download_debian_gnome_wallpaper() {
        Logger::step(_("gui.wallpaper.debian_gnome_download"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/debian/pool/main/g/gnome-backgrounds/",
                                 "gnome-backgrounds.*all\\.deb", "gnome_bg");
    }

    void BeautificationManager::download_ubuntu_kylin_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_kylin_download"));
        fetch_latest_and_extract("https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntukylin-wallpapers/",
                                 "ubuntukylin-wallpapers_.*\\.tar\\.xz", "ukylin_wp");
    }

    void BeautificationManager::download_ubuntu_wallpaper(const std::string &ubuntu_code) {
        Logger::step(std::string(_("gui.wallpaper.ubuntu_download")) + ubuntu_code);
        std::string home = user_home();
        fs::path wp_dir = fs::path(home) / "Pictures" / "ubuntu-wallpapers";
        fs::create_directories(wp_dir);

        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-wallpapers/";
        std::string pattern = "ubuntu-wallpapers-" + ubuntu_code + ".*all\\.deb";
        std::string html = http_get_cached(repo, ".ubuntu_wp_index.html");
        if (html.empty()) return;

        std::string pkg_name = find_latest_href(html, pattern);
        if (pkg_name.empty()) return;

        std::string pkg_path = wp_dir.string() + "/ubuntu-wallpaper.deb";
        if (download_file(repo + pkg_name, pkg_path)) {
            extract_archive(pkg_path, wp_dir.string());
            std::error_code ec;
            fs::remove(pkg_path, ec);
            fs::remove(wp_dir.string() + "/data.tar.xz", ec);
        }
    }

    void BeautificationManager::link_to_debian_wallpaper() {
        std::string home = user_home();
        fs::create_directories(home + "/Pictures");
        if (fs::exists("/usr/share/backgrounds/kali/"))
            Executor::shell("ln -sf /usr/share/backgrounds/kali/ " + home + "/Pictures/kali 2>/dev/null || true");
        if (fs::exists("/usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/"))
            Executor::shell("ln -sf /usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/ " +
                            home + "/Pictures/debian-moonlight 2>/dev/null || true");
    }

    void BeautificationManager::check_theme_url(std::string &url) {
        if (url.find("www") != std::string::npos && url.find("http") == std::string::npos)
            url = "https://" + url;
    }

    // ═══════════════════════════════════════════════════════════════
    // 光标主题 (原生 C++)
    // ═══════════════════════════════════════════════════════════════

    void BeautificationManager::configure_mouse_cursor() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.cursor.menu_title"))
                    .add_arg("--menu").add_arg(_("gui.cursor.menu_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.cursor.all_modern_item"))
                    .add_arg("2").add_arg(_("gui.cursor.breeze_item"))
                    .add_arg("3").add_arg(_("gui.cursor.chameleon_item"))
                    .add_arg("4").add_arg(_("gui.cursor.moblin_item"))
                    .add_arg("5").add_arg(_("gui.cursor.install_via_pkg_item"))
                    .add_arg("0").add_arg(_("menu.tui.back"))
                    .build_string();

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") {
                download_chameleon_cursor_theme();
            } else if (ch == "2" || ch == "3" || ch == "4") {
                const char *name = nullptr;
                const char *repo = nullptr;
                if (ch == "2") {
                    name = "breeze-cursor-theme";
                    repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/";
                } else if (ch == "3") {
                    name = "chameleon-cursor-theme";
                    repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/";
                } else if (ch == "4") {
                    name = "moblin-cursor-theme";
                    repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/";
                }

                Logger::step(std::string(_("gui.cursor.install_step")) + name);
                std::string pattern = std::string(name) + ".*all\\.deb";
                if (!fetch_latest_and_extract(repo, pattern, std::string(name) + "_dl")) {
                    // bfsu 回退到 debian 官方源
                    std::string fallback = repo;
                    size_t p = fallback.find("mirrors.bfsu.edu.cn");
                    if (p != std::string::npos) fallback.replace(p, 21, "ftp.debian.org");
                    fetch_latest_and_extract(fallback, pattern, std::string(name) + "_dl_fb");
                }
                Logger::ok(_("gui.cursor.install_done"));
            } else if (ch == "5") {
                std::string pkg_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.cursor.pkg_install_title"))
                        .add_arg("--inputbox").add_arg(_("gui.cursor.pkg_install_prompt"))
                        .add_arg("0").add_arg("50").add_arg("breeze-cursor-theme")
                        .build_string();
                std::string pkg_name = Executor::tui_select(pkg_cmd);
                if (!pkg_name.empty()) install_cursor_theme(pkg_name);
            }

            Logger::info(_("gui.cursor.apply_hint"));
            Logger::press_enter();
        }
    }

    void BeautificationManager::download_chameleon_cursor_theme() {
        const std::pair<const char *, const char *> cursors[] = {
            {"breeze-cursor-theme",    "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/"},
            {"chameleon-cursor-theme", "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/"},
            {"moblin-cursor-theme",    "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/"},
        };
        for (const auto &c : cursors) {
            fetch_latest_and_extract(c.second,
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
            download_file(url, path);
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
        if (!download_file(url, tmp))
            download_file(url_02, tmp);
        if (fs::exists(tmp)) {
            extract_archive(tmp);
            std::error_code ec;
            fs::remove(tmp, ec);
        }
    }

} // namespace tmoe::domain
