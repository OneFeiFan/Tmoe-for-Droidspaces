#include "beautification_manager.h"
#include "desktop_manager.h"
#include "domain/system/package_manager.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "core/command_builder.hpp"
#include <filesystem>


namespace tmoe::domain {
    namespace {
        DistroFamily get_family(const TmoeConfig &cfg) {
            auto f = infer_family_from_config(cfg.linux_distro);
            if (f == DistroFamily::Unknown)
                f = PackageManager::detect_distro_family();
            return f;
        }
    }

    void BeautificationManager::run_beautification_menu() {
        // 对应 Bash: tmoe_desktop_beautification — 6项菜单
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
                download_chameleon_cursor_theme();
            } else if (choice == "5") {
                install_dock();
            } else if (choice == "6") {
                install_compiz();
            }
            Logger::press_enter();
        }
    }

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

    bool BeautificationManager::install_dock() const {
        Logger::step(_("gui.dock.install_step"));

        // 1. 安装 plank
        if (!PackageManager::install({"plank"}, get_family(cfg_))) {
            Logger::warn(_("gui.dock.install_failed"));
            return false;
        }

        // 2. 创建 autostart 条目
        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        std::string autostart_dir = home + "/.config/autostart";
        fs::create_directories(autostart_dir);
        std::string desktop_entry =
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Plank\n"
                "Exec=plank\n"
                "X-GNOME-Autostart-enabled=true\n"
                "OnlyShowIn=XFCE;LXDE;MATE;Budgie;\n";
        SystemHelper::write_file(fs::path(autostart_dir + "/plank.desktop"), desktop_entry);
        Logger::info(_("gui.dock.autostart_created"));

        // 3. 下载 anti-snap plank 主题（轻量级、无 snap 依赖）
        std::string themes_dir = home + "/.local/share/plank/themes";
        if (!fs::exists(themes_dir + "/anti-snap")) {
            fs::create_directories(themes_dir);
            std::string tmp_clone = "/tmp/.plank_anti_snap_theme";
            std::error_code ec;
            fs::remove_all(tmp_clone, ec);
            Executor::shell("cd /tmp && "
                            "(git clone --depth=1 https://github.com/TaylanTatli/Anti-Snap.git "
                            + tmp_clone + " 2>/dev/null || "
                            "git clone --depth=1 git://github.com/TaylanTatli/Anti-Snap.git "
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

        std::string url;
        std::string filename;
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
            Logger::info(
                std::string(_("gui.wallpaper.unknown_source")) + std::string(source) + _("gui.wallpaper.skip"));
            return false;
        }

        if (!url.empty()) {
            auto wp_dl = CommandBuilder("wget").add_arg("-q").add_arg(url)
                         .add_arg("-O").add_arg(wallpaper_dir + "/" + filename).build_string()
                         + " 2>/dev/null || " +
                         CommandBuilder("curl").add_arg("-sL").add_arg(url)
                         .add_arg("-o").add_arg(wallpaper_dir + "/" + filename).build_string()
                         + " 2>/dev/null";
            Executor::passthrough(wp_dl);
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

        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        std::string conky_dir = home + "/.config/conky";
        fs::create_directories(conky_dir);

        std::string conky_conf =
                "conky.config = {\n"
                "    alignment = 'top_right',\n"
                "    background = false,\n"
                "    double_buffer = true,\n"
                "    font = 'Iosevka:size=10',\n"
                "    gap_x = 12, gap_y = 48,\n"
                "    own_window = true,\n"
                "    own_window_type = 'desktop',\n"
                "    own_window_transparent = true,\n"
                "    own_window_argb_visual = true,\n"
                "    update_interval = 1.0,\n"
                "    use_xft = true,\n"
                "};\n\n"
                "conky.text = [[\n"
                "${color #00ff00}Host: ${color white}$nodename\n"
                "${color #00ff00}Kernel: ${color white}$kernel\n"
                "${color #00ff00}Uptime: ${color white}$uptime\n\n"
                "${color #ff6600}CPU: ${color white}${cpu}%\n"
                "${cpugraph 20 200}\n"
                "${color #ff6600}RAM: ${color white}$mem/$memmax\n"
                "${color #ff6600}Disk: ${color white}${fs_used /}/${fs_size /}\n"
                "${color #4080ff}Net: ${color white}${addr wlan0} ${addr eth0}\n"
                "]];\n";
        SystemHelper::write_file(fs::path(conky_dir + "/conky.conf"), conky_conf);

        std::string autostart_dir = home + "/.config/autostart";
        fs::create_directories(autostart_dir);
        std::string autostart = autostart_dir + "/conky.desktop";
        std::string desktop_entry =
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Conky\n"
                "Exec=conky -c " + conky_dir + "/conky.conf\n"
                "X-GNOME-Autostart-enabled=true\n";
        SystemHelper::write_file(fs::path(autostart), desktop_entry);

        Logger::ok(_("gui.conky.install_done"));

        if (!fs::exists(home + "/github/Harmattan")) {
            fs::create_directories(home + "/github");
            Executor::shell("cd " + home + "/github && "
                            "(git clone --depth=1 https://github.com/zagortenay333/Harmattan.git 2>/dev/null || "
                            "git clone --depth=1 git://github.com/zagortenay333/Harmattan.git 2>/dev/null || true)");
            if (fs::exists(home + "/github/Harmattan")) {
                Logger::info(std::string(_("gui.conky.harmattan_downloaded")) + home + "/github/Harmattan");
                Logger::info(_("gui.conky.harmattan_preview_hint"));
            }
        }

        return true;
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

        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
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

        std::string panel_xml = generate_xfce_panel_xml();
        return SystemHelper::write_file(panel_file, panel_xml);
    }

    std::string BeautificationManager::generate_xfce_panel_xml() {
        return gui_config::XFCE_PANEL_XML;
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 2: 额外的内联脚本生成
    // ═══════════════════════════════════════════════════════════════

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


    void BeautificationManager::download_uos_icon_theme() {
        PackageManager::install("deepin-icon-theme", get_family(cfg_));
        if (fs::exists("/usr/share/icons/Uos")) {
            Logger::info(_("gui.icon.uos_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.uos_download_step"));
        Executor::shell("git clone -b Uos --depth=1 https://gitee.com/mo2/xfce-themes.git "
            "/tmp/UosICONS 2>/dev/null && "
            "cd /tmp/UosICONS && "
            "tar -Jxvf Uos.tar.xz -C /usr/share/icons 2>/dev/null && "
            "update-icon-caches /usr/share/icons/Uos 2>/dev/null &");
        std::error_code ec;
        fs::remove_all("/tmp/UosICONS", ec);
        desktop_manager_.set_default_xfce_icon_theme("Uos");
    }


    void BeautificationManager::download_deepin_wallpaper() {
        Logger::step(_("gui.wallpaper.deepin_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/deepin/pool/main/d/deepin-wallpapers/";
        Executor::shell("cd /tmp && for GREP_NAME in 'deepin-community-wallpapers' 'deepin-wallpapers_'; do "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep \"$GREP_NAME\" | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o deepin.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv deepin.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null); done || true");
    }


    void BeautificationManager::xfce_theme_parsing() {
        std::string url_cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.theme.parser_title"))
                .add_arg("--inputbox")
                .add_arg(_("gui.theme.parser_prompt"))
                .add_arg("0").add_arg("50")
                .build_string();
        std::string url = Executor::tui_select(url_cmd);
        if (url.empty()) return;

        Logger::info(_("gui.theme.parsing_page"));
        Executor::shell("cd /tmp && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o .theme_index_cache_tmoe.html '" + url + "' 2>/dev/null || "
                        "curl -L -o .theme_index_cache_tmoe.html '" + url + "' 2>/dev/null || true");
        Executor::shell("cd /tmp && "
            "cat .theme_index_cache_tmoe.html | sed 's@,@\\n@g' | grep -E 'tar.xz|tar.gz' | grep '\"title\"' | "
            "sed 's@\"@ @g' | awk '{print $3}' | sort -um >.tmoe-linux_cache.01; "
            "THEME_LINE=$(cat .tmoe-linux_cache.01 | wc -l); "
            "cat .theme_index_cache_tmoe.html | sed 's@,@\\n@g' | sed 's@%2F@/@g' | sed 's@%3A@:@g' | "
            "sed 's@%2B@+@g' | sed 's@%3D@=@g' | sed 's@%23@#@g' | sed 's@%26@\\&@g' | "
            "grep -E '\"downloaded_count\"' | sed 's@\"@ @g' | awk '{print $3}' | head -n ${THEME_LINE} | "
            "sed 's/ /-/g' | sed 's/$/次/g' >.tmoe-linux_cache.02; "
            "paste -d ' ' .tmoe-linux_cache.01 .tmoe-linux_cache.02 >.tmoe-linux_cache.03 2>/dev/null || true");

        auto themes_result = Executor::shell("cat /tmp/.tmoe-linux_cache.03 2>/dev/null");
        if (!themes_result.ok() || themes_result.stdout_data.empty()) {
            Logger::info(_("gui.theme.no_themes_found"));
            return;
        }

        Logger::info(_("gui.theme.found_themes"));
        std::string themes = themes_result.stdout_data;
        std::istringstream iss(themes);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) Logger::info("  " + line);
        }

        Logger::info(_("gui.theme.manual_select_hint"));
    }


    void BeautificationManager::download_win10x_theme() {
        if (fs::exists("/usr/share/icons/We10X-Valley-dark")) {
            Logger::info(_("gui.icon.win10x_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.win10x_download_step"));
        Executor::shell("git clone -b win10x --depth=1 https://gitee.com/mo2/xfce-themes.git "
            "/tmp/.WINDOWS_11_ICON_THEME 2>/dev/null && "
            "cd /tmp/.WINDOWS_11_ICON_THEME && "
            "tar -Jxvf We10X.tar.xz -C /usr/share/icons 2>/dev/null && "
            "update-icon-caches /usr/share/icons/We10X-Valley-dark /usr/share/icons/We10X-Valley 2>/dev/null &");
        std::error_code ec;
        fs::remove_all("/tmp/.WINDOWS_11_ICON_THEME", ec);
        desktop_manager_.set_default_xfce_icon_theme("We10X-Valley");
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



    void BeautificationManager::download_arch_xfce_artwork() {
        Logger::step(_("gui.wallpaper.arch_xfce_artwork"));
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/extra/os/x86_64/";
        std::error_code ec;
        fs::path tmp_dir = "/tmp/.xfce_art";
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'xfce4-artwork' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo art.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf art.pkg.tar.xz -C / 2>/dev/null");
        fs::remove_all(tmp_dir, ec);
    }


    void BeautificationManager::download_manjaro_wallpaper() {
        Logger::step(_("gui.wallpaper.manjaro_download"));
        std::error_code ec;
        fs::path tmp_dir = "/tmp/.manjaro_wp";
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'wp2018.tar.xz' "
                        "'https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/wallpapers-2018-1.2-1-any.pkg.tar.xz' 2>/dev/null && "
                        "tar -Jxvf wp2018.tar.xz -C / 2>/dev/null && "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'wp2017.tar.xz' "
                        "'https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/manjaro-sx-wallpapers-20171023-1-any.pkg.tar.xz' 2>/dev/null && "
                        "tar -Jxvf wp2017.tar.xz -C / 2>/dev/null");
        fs::remove_all(tmp_dir, ec);
    }


    void BeautificationManager::download_candy_icon_theme() {
        if (fs::exists("/usr/share/icons/candy-icons")) {
            Logger::info(_("gui.icon.candy_already_installed"));
            return;
        }
        Logger::step(_("gui.icon.candy_download_step"));
        std::error_code ec;
        fs::path tmp_dir = "/tmp/.CANDY_ICON_THEME";
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "(curl -Lo master.zip https://ghproxy.com/https://github.com/EliverLara/candy-icons/"
                        "archive/refs/heads/master.zip 2>/dev/null || "
                        "curl -Lo master.zip https://github.com/EliverLara/candy-icons/"
                        "archive/refs/heads/master.zip 2>/dev/null) && "
                        "unzip master.zip 2>/dev/null && "
                        "mv candy-icons-master /usr/share/icons/candy-icons 2>/dev/null && "
                        "update-icon-caches /usr/share/icons/candy-icons 2>/dev/null &");
        fs::remove_all(tmp_dir, ec);
        desktop_manager_.set_default_xfce_icon_theme("candy-icons");
    }


    void BeautificationManager::check_theme_url(std::string &url) {
        if (url.find("www") != std::string::npos && url.find("http") == std::string::npos) {
            url = "https://" + url;
        }
    }


    void BeautificationManager::download_ubuntu_wallpaper(const std::string &ubuntu_code) {
        Logger::step(std::string(_("gui.wallpaper.ubuntu_download")) + ubuntu_code);
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        fs::path wp_dir = fs::path(home) / "Pictures" / "ubuntu-wallpapers";
        fs::create_directories(wp_dir);

        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-wallpapers/";
        Executor::shell("cd " + wp_dir.string() + " && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'ubuntu-wallpapers-" + ubuntu_code + "' | "
                        "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o ubuntu-wallpaper.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv ubuntu-wallpaper.deb 2>/dev/null; tar -Jxvf data.tar.xz -C . 2>/dev/null) || true");
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

        if (!fs::exists("/tmp/LXDE_BUSYeSLZRqq3i3oM.png"))
            Executor::shell("cd /tmp && " +
                            CommandBuilder("curl").add_arg("-sLo").add_arg("LXDE_BUSYeSLZRqq3i3oM.png").add_arg(
                                lxde_url)
                            .build_string() + " 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/LXDE_BUSYeSLZRqq3i3oM.png"))
            Executor::passthrough(CommandBuilder("catimg").add_arg("/tmp/LXDE_BUSYeSLZRqq3i3oM.png")
                                  .build_string() + " 2>/dev/null || true");

        if (!fs::exists("/tmp/MATE_1frRp1lpOXLPz6mO.jpg"))
            Executor::shell("cd /tmp && " +
                            CommandBuilder("curl").add_arg("-sLo").add_arg("MATE_1frRp1lpOXLPz6mO.jpg").add_arg(
                                mate_url)
                            .build_string() + " 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/MATE_1frRp1lpOXLPz6mO.jpg"))
            Executor::passthrough(CommandBuilder("catimg").add_arg("/tmp/MATE_1frRp1lpOXLPz6mO.jpg")
                                  .build_string() + " 2>/dev/null || true");

        if (!fs::exists("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg"))
            Executor::shell("cd /tmp && " +
                            CommandBuilder("curl").add_arg("-sLo").add_arg("XFCE_a7IQ9NnfgPckuqRt.jpg").add_arg(
                                xfce_url)
                            .build_string() + " 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg"))
            Executor::passthrough(CommandBuilder("catimg").add_arg("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg")
                                  .build_string() + " 2>/dev/null || true");

        if (cfg_.is_wsl) {
            fs::create_directories("/mnt/c/Users/Public/Downloads/VcXsrv");
            std::error_code ec;
            fs::copy_file("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg",
                          "/mnt/c/Users/Public/Downloads/VcXsrv/XFCE_a7IQ9NnfgPckuqRt.jpg",
                          fs::copy_options::overwrite_existing, ec);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 7: x11vnc / XRDP 增强
    // ═══════════════════════════════════════════════════════════════


    void BeautificationManager::tmoe_theme_installer(const std::string &file_path, bool is_icon) {
        std::string extract_path = is_icon ? "/usr/share/icons" : "/usr/share/themes";
        fs::create_directories(extract_path);
        auto tar_cmd = CommandBuilder("tar").add_arg("-xf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        auto tar_j_cmd = CommandBuilder("tar").add_arg("-Jxf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        Executor::passthrough(tar_cmd + " 2>/dev/null || " + tar_j_cmd + " 2>/dev/null || true");
        Logger::ok(std::string(_("gui.theme.installed_to")) + extract_path);
    }


    void BeautificationManager::download_wallpapers_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.wallpaper.menu_title"))
                    .add_arg("--menu")
                    .add_arg(_("gui.wallpaper.menu_prompt"))
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


    void BeautificationManager::download_icon_themes_menu() {
        while (true) {
            desktop_manager_.check_update_icon_caches_sh();
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.icon.menu_title"))
                    .add_arg("--menu")
                    .add_arg(_("gui.icon.menu_prompt"))
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
                // 下载全部 3 款现代光标主题
                download_chameleon_cursor_theme();
            } else if (ch == "2" || ch == "3" || ch == "4") {
                // 单独下载某一款光标主题
                const char *name = nullptr;
                const char *url = nullptr;
                if (ch == "2") {
                    name = "breeze-cursor-theme";
                    url = "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/";
                } else if (ch == "3") {
                    name = "chameleon-cursor-theme";
                    url = "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/";
                } else if (ch == "4") {
                    name = "moblin-cursor-theme";
                    url = "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/";
                }

                Logger::step(std::string(_("gui.cursor.install_step")) + name);
                std::string fallback = std::string(url);
                size_t bfsu_pos = fallback.find("mirrors.bfsu.edu.cn");
                if (bfsu_pos != std::string::npos) {
                    fallback.replace(bfsu_pos, 21, "ftp.debian.org");
                }
                Executor::shell("cd /tmp && "
                               "LATEST=$(curl -L '" + std::string(url) + "' 2>/dev/null | grep '" +
                               name + "' | grep all.deb | tail -n 1 | "
                               "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                               "[ -n \"$LATEST\" ] && (aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                               "-o cursor.deb '" + std::string(url) + "'\"$LATEST\" 2>/dev/null || "
                               "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                               "-o cursor.deb '" + fallback + "'\"$LATEST\" 2>/dev/null) && "
                               "(ar xv cursor.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
                Logger::ok(_("gui.cursor.install_done"));
            } else if (ch == "5") {
                // 通过包管理器安装
                std::string pkg_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.cursor.pkg_install_title"))
                        .add_arg("--inputbox")
                        .add_arg(_("gui.cursor.pkg_install_prompt"))
                        .add_arg("0").add_arg("50")
                        .add_arg("breeze-cursor-theme")
                        .build_string();
                std::string pkg_name = Executor::tui_select(pkg_cmd);
                if (!pkg_name.empty()) {
                    install_cursor_theme(pkg_name);
                }
            }

            Logger::info(_("gui.cursor.apply_hint"));
            Logger::press_enter();
        }
    }


    void BeautificationManager::download_and_cat_icon_img(const std::string &url, const std::string &filename) {
        if (!fs::exists("/tmp/" + filename)) {
            Executor::shell("cd /tmp && " +
                            CommandBuilder("curl").add_arg("-sLo").add_arg(filename).add_arg(url)
                            .build_string() + " 2>/dev/null || true");
        }
        if (Executor::has("catimg") && fs::exists("/tmp/" + filename)) {
            Executor::passthrough(CommandBuilder("catimg").add_arg("/tmp/" + filename)
                                  .build_string() + " 2>/dev/null || true");
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


    void BeautificationManager::download_debian_gnome_wallpaper() {
        Logger::step(_("gui.wallpaper.debian_gnome_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/g/gnome-backgrounds/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'gnome-backgrounds' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o gnome-bg.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv gnome-bg.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
    }


    void BeautificationManager::link_to_debian_wallpaper() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        fs::create_directories(home + "/Pictures");
        if (fs::exists("/usr/share/backgrounds/kali/")) {
            Executor::shell("ln -sf /usr/share/backgrounds/kali/ " + home + "/Pictures/kali 2>/dev/null || true");
        }
        if (fs::exists("/usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/")) {
            Executor::shell("ln -sf /usr/share/desktop-base/moonlight-theme/wallpaper/contents/images/ " +
                            home + "/Pictures/debian-moonlight 2>/dev/null || true");
        }
    }


    void BeautificationManager::download_raspbian_pixel_assets() {
        // 树莓派 Pixel 桌面素材包 — 包含壁纸和图标，两个菜单共用
        Logger::step(_("gui.pixel.download_step"));

        std::string repo = "https://mirrors.bfsu.edu.cn/raspberrypi/pool/ui/p/pixel-wallpaper/";
        std::string fallback_repo = "https://mirrors.tuna.tsinghua.edu.cn/raspberrypi/pool/ui/p/pixel-wallpaper/";

        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'pixel-wallpaper' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && (aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o pixel.deb '" + repo + "'\"$LATEST\" 2>/dev/null || "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o pixel.deb '" + fallback_repo + "'\"$LATEST\" 2>/dev/null) && "
                        "(ar xv pixel.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");

        // 更新图标缓存（如果包内包含图标主题）
        desktop_manager_.check_update_icon_caches_sh();
        if (fs::exists("/usr/share/icons/PiX")) {
            Executor::shell("update-icon-caches /usr/share/icons/PiX 2>/dev/null &");
        }
        if (fs::exists("/usr/share/icons/raspberrypi")) {
            Executor::shell("update-icon-caches /usr/share/icons/raspberrypi 2>/dev/null &");
        }

        // 链接壁纸到用户 Pictures 目录
        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        if (fs::exists("/usr/share/backgrounds/pixel")) {
            fs::create_directories(home + "/Pictures");
            Executor::shell("ln -sf /usr/share/backgrounds/pixel " + home +
                            "/Pictures/raspberrypi-pixel-wallpapers 2>/dev/null || true");
            Logger::info(_("gui.pixel.wallpaper_linked"));
        }

        Logger::ok(_("gui.pixel.download_done"));
    }


    void BeautificationManager::local_theme_installer() {
        Logger::info(_("gui.theme.local_installer"));
        Logger::info(_("gui.theme.place_in_tmp"));

        std::string cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.theme.local_installer_title"))
                .add_arg("--inputbox")
                .add_arg(_("gui.theme.local_installer_prompt"))
                .add_arg("10").add_arg("60")
                .build_string();
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
            .add_arg("0").add_arg("50")
            .build_string());
        bool is_icon = (type_r.exit_code == 1);

        tmoe_theme_installer(full_path, is_icon);
    }


    void BeautificationManager::download_elementary_wallpaper() {
        Logger::step(_("gui.wallpaper.elementary_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/pool/community/";
        std::error_code ec;
        fs::path tmp_dir = "/tmp/.elementary_wp";
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'elementary-wallpapers' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo wp.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf wp.pkg.tar.xz -C / 2>/dev/null");
        fs::remove_all(tmp_dir, ec);
    }


    void BeautificationManager::download_paper_icon_theme() {
        Logger::step(_("gui.icon.paper_download_step"));
        std::string repo = "https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'paper-icon-theme' | "
                        "grep pkg.tar | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o paper.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf paper.pkg.tar.xz -C / 2>/dev/null && "
                        "update-icon-caches /usr/share/icons/Paper /usr/share/icons/Paper-Mono-Dark 2>/dev/null &");
        desktop_manager_.set_default_xfce_icon_theme("Paper");
    }


    void BeautificationManager::download_manjaro_pkg(const std::string &theme_name,
                                                     const std::string &url,
                                                     const std::string &url_02,
                                                     const std::string & /*wallpaper_name*/,
                                                     const std::string & /*custom_name*/) {
        std::error_code ec;
        fs::path tmp_dir = "/tmp/." + theme_name;
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'data.tar.xz' '" + url + "' 2>/dev/null || "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'data.tar.xz' '" + url_02 + "' 2>/dev/null) && "
                        "tar -Jxvf data.tar.xz -C / 2>/dev/null");
        fs::remove_all(tmp_dir, ec);
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 4: FVWM 特殊安装 / DM systemctl
    // ═══════════════════════════════════════════════════════════════


    void BeautificationManager::download_arch_wallpaper() {
        link_to_debian_wallpaper();
        Logger::step(_("gui.wallpaper.arch_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/pool/community/";
        std::error_code ec;
        fs::path tmp_dir = "/tmp/.arch_wp";
        fs::create_directories(tmp_dir, ec);
        Executor::shell("cd " + tmp_dir.string() + " && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'archlinux-wallpaper' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo wp.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf wp.pkg.tar.xz -C / 2>/dev/null");
        fs::remove_all(tmp_dir, ec);
    }


    void BeautificationManager::configure_theme_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.theme.menu_title"))
                    .add_arg("--menu")
                    .add_arg(_("gui.theme.menu_prompt"))
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
                    std::error_code ec;
                    fs::path tmp_dir = "/tmp/.ukui-gtk-themes";
                    fs::create_directories(tmp_dir, ec);
                    Executor::shell("cd " + tmp_dir.string() + " && "
                                    "UKUITHEME=\"$(curl -LfsS 'https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/' 2>/dev/null | "
                                    "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2)\" && "
                                    "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                                    "-o 'ukui-themes.deb' \"https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/$UKUITHEME\" && "
                                    "ar xv ukui-themes.deb && cd / && "
                                    "tar -Jxvf " + tmp_dir.string() + "/data.tar.xz ./usr 2>/dev/null && "
                                    "update-icon-caches /usr/share/icons/ukui-icon-theme-basic "
                                    "/usr/share/icons/ukui-icon-theme-classical "
                                    "/usr/share/icons/ukui-icon-theme-default 2>/dev/null &");
                    fs::remove_all(tmp_dir, ec);
                }
                desktop_manager_.set_default_xfce_icon_theme("ukui-icon-theme");
            } else if (ch == "9") desktop_manager_.install_arc_gtk_theme_ext();
            Logger::press_enter();
        }
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







    void BeautificationManager::download_chameleon_cursor_theme() {
        for (const auto &info: {
                 std::make_pair("breeze-cursor-theme",
                                "https://mirrors.bfsu.edu.cn/debian/pool/main/b/breeze/"),
                 std::make_pair("chameleon-cursor-theme",
                                "https://mirrors.bfsu.edu.cn/debian/pool/main/c/chameleon-cursor-theme/"),
                 std::make_pair("moblin-cursor-theme",
                                "https://mirrors.bfsu.edu.cn/debian/pool/main/m/moblin-cursor-theme/")
             }) {
            Executor::shell("cd /tmp && "
                            "LATEST=$(curl -L '" + std::string(info.second) + "' 2>/dev/null | grep '" +
                            std::string(info.first) + "' | grep all.deb | tail -n 1 | "
                            "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                            "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                            "-o cursor.deb '" + std::string(info.second) + "'\"$LATEST\" && "
                            "(ar xv cursor.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 6: WSL 完整支持
    // ═══════════════════════════════════════════════════════════════


    void BeautificationManager::download_ubuntu_kylin_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_kylin_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntukylin-wallpapers/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'ubuntukylin-wallpapers_' | "
                        "grep '.tar.xz' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o ukylin.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf ukylin.tar.xz -C / 2>/dev/null || true");
    }


} // namespace tmoe::domain
