#include "desktop_manager.h"

namespace tmoe::domain {
    DesktopManager::DesktopManager(const TmoeConfig &cfg, VncManager &vnc_manager)
        : cfg_(cfg), vnc_manager_(vnc_manager) {
    }

    DistroFamily DesktopManager::resolved_family() const {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        return family;
    }

    bool DesktopManager::install_packages(const std::vector<std::string> &pkgs) const {
        return PackageManager::install(pkgs, resolved_family());
    }

    bool DesktopManager::remove_packages(const std::vector<std::string> &pkgs) const {
        return PackageManager::remove(pkgs, resolved_family());
    }

    void DesktopManager::set_auto_install_flags(bool fcitx, bool electron, bool vscode,
                                                bool chromium, bool kali,
                                                const std::string &kali_tools) {
        auto_install_fcitx4_ = fcitx;
        auto_install_electron_ = electron;
        auto_install_vscode_ = vscode;
        auto_install_chromium_ = chromium;
        auto_install_kali_ = kali;
        kali_tools_ = kali_tools;
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面环境注册表
    // ═══════════════════════════════════════════════════════════════

    const std::vector<DesktopInfo> &DesktopManager::desktop_registry() const {
        return gui_config::all_desktops();
    }

    DesktopInfo DesktopManager::get_desktop_info(std::string_view desktop) const {
        for (const auto &info: desktop_registry()) {
            if (info.id == desktop) return info;
        }
        // 默认回退到 xfce
        return desktop_registry().front();
    }


    std::vector<DesktopInfo> DesktopManager::list_desktops() const {
        return desktop_registry();
    }


    void DesktopManager::preconfigure_gui_dependencies() {
        Logger::step(_("gui.preconfig_deps"));
        auto family = resolved_family();
        auto deps = gui_config::distro_gui_deps(PackageManager::family_key(family));

        // Gentoo 特殊: 先运行 dispatch-conf etc-update
        if (family == DistroFamily::Gentoo) {
            Executor::shell("dispatch-conf 2>/dev/null || true");
            Executor::shell("etc-update 2>/dev/null || true");
        }

        // 辅助: 将空格分隔的包名转为 vector
        auto split_pkgs = [](const std::string &s) -> std::vector<std::string> {
            std::vector<std::string> out;
            std::istringstream iss(s);
            std::string token;
            while (iss >> token) out.push_back(token);
            return out;
        };

        // 安装核心包 (使用 PackageManager 以获得 eatmydata/回退等完整逻辑)
        if (!deps.extra_pkgs.empty())
            PackageManager::install(split_pkgs(deps.extra_pkgs), family);
        if (!deps.vnc_pkg.empty() && !deps.skip_vnc)
            PackageManager::install(split_pkgs(deps.vnc_pkg), family);
        if (!deps.font_pkg.empty())
            PackageManager::install(split_pkgs(deps.font_pkg), family);

        Logger::ok(_("gui.preconfig_deps_done"));
    }


    void DesktopManager::will_be_installed_for_you(std::string_view desktop_session) {
        Logger::info(_f("gui.will_install", std::string(desktop_session)));
    }


    bool DesktopManager::install_desktop(std::string_view desktop) {
        DesktopInfo info = get_desktop_info(desktop);
        Logger::step(_f("gui.install.desktop", info.name));

        // 前置依赖 (对应旧 Bash preconfigure_gui_dependecies_02)
        preconfigure_gui_dependencies();

        // 检查是否需要 root 权限
        if (info.requires_root && cfg_.is_termux) {
            Logger::warn(_f("gui.desktop.systemd_warn", info.name));
        }

        // 提示即将安装的软件包
        will_be_installed_for_you(info.name);

        // 安装桌面环境包
        std::vector<std::string> pkgs;

        // 先检查是否有当前发行版的特定包名覆盖
        auto family = resolved_family();
        std::string distro_key = PackageManager::family_key(family);

        std::string pkg_list = info.pkg_group; // 默认包名
        if (!info.distro_pkgs.empty()) {
            auto it = info.distro_pkgs.find(distro_key);
            if (it != info.distro_pkgs.end()) {
                pkg_list = it->second;
            }
        }

        std::istringstream iss(pkg_list);
        std::string pkg;
        while (iss >> pkg) pkgs.emplace_back(pkg);

        // 附加通用包
        pkgs.emplace_back("dbus-x11");

        if (!install_packages(pkgs)) {
            Logger::error(_f("gui.install.fail", info.name));
            return false;
        }

        // 桌面专属 post-install (xfce/kde/gnome/mate/lxde/lxqt/cinnamon/budgie/dde/deepin/ukui)
        post_install_desktop_config(desktop);

        // 配置 VNC xstartup
        if (!vnc_manager_.configure_xstartup(desktop)) {
            Logger::warn(_("gui.desktop.xstartup_warn"));
        }

        Logger::ok(_f("gui.install.success", info.name));

        // 显示兼容性说明
        if (!info.compat_notes.empty()) {
            Logger::info(_f("gui.desktop.compat_notes", info.compat_notes));
        }

        // 根据桌面类型推荐 VNC 服务端
        std::string desktop_lower(desktop);
        std::transform(desktop_lower.begin(), desktop_lower.end(), desktop_lower.begin(), ::tolower);

        if (desktop_lower == "kde" || desktop_lower == "gnome" || desktop_lower == "cinnamon" ||
            desktop_lower == "dde" || desktop_lower == "ukui" || desktop_lower == "budgie") {
            Logger::info(_("gui.desktop.recommend_tiger"));
            vnc_manager_.config().server = "tiger";
            vnc_manager_.config().server_bin = "tigervnc";
        }

        // ── 对应旧 Bash do_you_want_to_install_fcitx4 追问链 ──
        post_desktop_install_prompts();

        return true;
    }


    void DesktopManager::select_kali_tools() {
        // 对应旧 Bash do_you_want_to_install_kali_tools
        // Kali Linux 工具包选择子菜单
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"KALI LINUX TOOLS\""
                               " --menu \"请选择要安装的 Kali Linux 工具包\\n"
                               "Select Kali Linux tools metapackage\" 0 0 0 "
                               "\"arm\" \"kali-linux-arm (ARM设备工具)\" "
                               "\"None\" \"不安装工具包 (Skip)\" "
                               "\"core\" \"kali-linux-core (核心工具)\" "
                               "\"default\" \"kali-linux-default (默认工具集)\" "
                               "\"everything\" \"kali-linux-everything (全部工具)\" "
                               "\"headless\" \"kali-linux-headless (无头模式)\" "
                               "\"large\" \"kali-linux-large (大型工具集)\" "
                               "\"nethunter\" \"kali-linux-nethunter (NetHunter)\" "
                               "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice == "0" || choice.empty() || choice == "None") return;

        // 映射选项到包名
        std::string pkg_name;
        if (choice == "arm") pkg_name = "kali-linux-arm";
        else if (choice == "core") pkg_name = "kali-linux-core";
        else if (choice == "default") pkg_name = "kali-linux-default";
        else if (choice == "everything") pkg_name = "kali-linux-everything";
        else if (choice == "headless") pkg_name = "kali-linux-headless";
        else if (choice == "large") pkg_name = "kali-linux-large";
        else if (choice == "nethunter") pkg_name = "kali-linux-nethunter";

        if (!pkg_name.empty()) {
            install_packages({pkg_name});
        }
    }


    void DesktopManager::post_desktop_install_prompts() {
        // 对应旧 Bash auto_install_and_configure_fcitx4 + do_you_want_to_install_fcitx4 追问链
        bool interactive = !auto_install_mode_;

        auto family = resolved_family();

        auto lang = std::string(I18n::current_lang());
        bool want_fcitx = false;
        bool want_chromium = auto_install_chromium_;
        bool want_electron = auto_install_electron_;
        bool want_kali = auto_install_kali_;

        // 1. fcitx: 中文环境, 仅 debian/arch, 非 WSL
        if ((family == DistroFamily::Debian || family == DistroFamily::Arch) && !cfg_.is_wsl &&
            lang.find("zh") == 0 && !Executor::has("fcitx") && !Executor::has("fcitx5")) {
            if (interactive) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"input method\""
                                               " --yesno '检测到您当前的语言环境为中文，是否需要安装中文输入法？\\n"
                                               "安装完成后，在桌面环境下按Ctrl+空格切换输入法\\n"
                                               "你也可以选择NO跳过，之后可以单独安装fcitx5' 0 0");
                want_fcitx = (r.exit_code == 0);
            } else {
                want_fcitx = auto_install_fcitx4_;
            }
        }

        // 2. Chromium
        if (!Executor::has("chromium") && !Executor::has("chromium-browser")
            && !Executor::has("google-chrome") && !Executor::has("google-chrome-stable")) {
            if (interactive) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"CHROMIUM-BROWSER\""
                                               " --yesno 'Do you want to install Google Chromium browser?' 0 0");
                want_chromium = (r.exit_code == 0);
            }
        }

        // 3. Electron 应用合集 (alpine 排除)
        if (family != DistroFamily::Alpine && !Executor::has("electron")) {
            if (interactive) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"Electron apps\""
                                               " --yesno '请问您是否需要安装开发者推荐的electron软件包合集？\\n"
                                               "该合集包含哔哩哔哩客户端，obsidian(markdown编辑器)，\\n"
                                               "网易云音乐第三方electron版，listen1，\\n"
                                               "YesPlayMusic，petal和zy-player\\n"
                                               "你可以选择NO跳过，之后可以单独安装electron app。' 0 0");
                want_electron = (r.exit_code == 0);
            }
        }

        // 4. Kali tools
        bool is_kali = (cfg_.linux_distro == "kali" || cfg_.linux_distro.find("kali") != std::string::npos);
        if (is_kali && interactive) {
            select_kali_tools();
        }

        // ── 执行安装 ──
        if (want_fcitx) install_fcitx();

        if (want_chromium) {
            install_packages({"chromium-browser", "chromium"});
        }

        if (want_electron) {
            install_packages({
                "bilibili", "electron-netease-cloud-music",
                "obsidian", "listen1", "yesplaymusic", "petal", "zy-player"
            });
        }

        if (is_kali && !interactive && auto_install_kali_) {
            // 静默模式安装 kali 工具
            install_packages({kali_tools_});
        }

        // VSCode 自动安装
        if (!interactive && auto_install_vscode_) {
            Executor::passthrough(cfg_.install_command + " code 2>/dev/null || "
                                  "snap install code --classic 2>/dev/null || true");
        }

        // Ubuntu language support
        if (cfg_.sub_distro == "ubuntu") {
            Executor::passthrough("apt install -y $(check-language-support) 2>/dev/null || true");
        }
    }


    void DesktopManager::plasma_wayland_env() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string profile_file = home + "/.profile";

        std::string existing = SystemHelper::read_file(fs::path(profile_file));

        bool has_xdg = (existing.find("XDG_SESSION_TYPE=wayland") != std::string::npos);
        bool has_gdk = (existing.find("GDK_BACKEND=wayland") != std::string::npos);
        bool has_qt = (existing.find("QT_QPA_PLATFORM=wayland") != std::string::npos);
        bool has_moz = (existing.find("MOZ_ENABLE_WAYLAND=1") != std::string::npos);

        std::string append_content;
        if (!has_xdg) append_content += "export XDG_SESSION_TYPE=wayland\n";
        if (!has_gdk) append_content += "export GDK_BACKEND=wayland\n";
        if (!has_qt) append_content += "export QT_QPA_PLATFORM=wayland\n";
        if (!has_moz) append_content += "export MOZ_ENABLE_WAYLAND=1\n";

        if (!append_content.empty()) {
            SystemHelper::append_file(fs::path(profile_file),
                                      "\n# tmoe-linux Plasma Wayland 环境变量\n" + append_content);
            Executor::shell("export XDG_SESSION_TYPE=wayland 2>/dev/null || true");
            Executor::shell("export GDK_BACKEND=wayland 2>/dev/null || true");
            Executor::shell("export QT_QPA_PLATFORM=wayland 2>/dev/null || true");
            Executor::shell("export MOZ_ENABLE_WAYLAND=1 2>/dev/null || true");
            Logger::ok(_("gui.plasma_wayland.configured"));
        } else {
            Logger::info(_("gui.plasma_wayland.exists"));
        }
    }


    void DesktopManager::post_install_desktop_config(std::string_view desktop) {
        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        auto family = resolved_family();
        bool is_debian = (family == DistroFamily::Debian);
        bool is_ubuntu = (is_debian && cfg_.sub_distro == "ubuntu");
        bool is_proot = cfg_.is_termux || (cfg_.linux_distro == "Android");

        // ── 系统维护 (所有桌面共用) ──
        if (is_debian) {
            Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
            if (!Executor::shell("dpkg -l keyboard-configuration 2>/dev/null | grep -q '^ii'").ok())
                install_packages({"keyboard-configuration"});
            vnc_manager_.auto_select_keyboard_layout();
        }

        // ── apt_purge_libfprint + remove_udisk_and_gvfs (proot+debian) ──
        if (is_proot && is_debian) {
            Executor::passthrough("apt purge -y ^libfprint 2>/dev/null || true");
            Executor::passthrough("apt clean 2>/dev/null || true");
            Executor::passthrough("apt autoclean 2>/dev/null || true");
            if (d.find("xfce") != std::string::npos || d.find("lxde") != std::string::npos ||
                d.find("lxqt") != std::string::npos) {
                remove_packages({"udisks2", "gvfs", "gvfs-backends", "gvfs-daemons"});
            }
        }

        // ── 按桌面类型分发 ──
        if (d.find("xfce") != std::string::npos)
            post_install_xfce(family, is_debian, is_ubuntu, is_proot);
        else if (d.find("kde") != std::string::npos)
            post_install_kde(family, is_debian, is_ubuntu, is_proot);
        else if (d.find("gnome") != std::string::npos)
            post_install_gnome(family, is_debian, is_ubuntu);
        else if (d.find("mate") != std::string::npos)
            post_install_mate(family, is_debian, is_ubuntu);
        else if (d.find("lxqt") != std::string::npos)
            post_install_lxqt(is_debian, is_ubuntu);
        else if (d.find("lxde") != std::string::npos)
            post_install_lxde(is_debian);
        else if (d.find("cinnamon") != std::string::npos)
            post_install_cinnamon(family, is_debian);
        else if (d.find("budgie") != std::string::npos)
            post_install_budgie(is_debian, is_ubuntu);
        else if (d.find("dde") != std::string::npos || d.find("deepin") != std::string::npos)
            post_install_dde_or_deepin(family, is_debian);
        else if (d.find("ukui") != std::string::npos)
            post_install_ukui(is_ubuntu);
        else if (d.find("cutefish") != std::string::npos)
            post_install_cutefish(family, is_debian, is_ubuntu, is_proot);
    }

    // ═══════════════════════════════════════════════════════════════
    // Per-desktop post_install handlers
    // ═══════════════════════════════════════════════════════════════

    void DesktopManager::post_install_xfce(DistroFamily family, bool is_debian, bool is_ubuntu, bool is_proot) {
        xfce_warning();
        if (is_ubuntu) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"Xfce or Xubuntu-desktop\" --yes-button \"xfce\" --no-button \"xubuntu\""
                                           " --yesno '前者为普通xfce,后者为xubuntu' 0 0");
            if (r.exit_code == 1) {
                install_packages({"xubuntu-desktop", "xubuntu-default-settings"});
                if (cfg_.is_termux && is_debian) vnc_manager_.fix_mlocate();
            }
        }
        // debian_xfce4_extras
        if (is_debian) {
            install_packages({
                "xfce4-whiskermenu-plugin", "xfce4-taskmanager",
                "xfce4-places-plugin", "xfce4-netload-plugin", "xfce4-battery-plugin",
                "xfce4-datetime-plugin", "xfce4-verve-plugin", "xfce4-mount-plugin",
                "xfce4-screenshooter", "xfce4-clipman-plugin", "xfce4-pulseaudio-plugin",
                "thunar-archive-plugin", "gvfs", "gvfs-backends", "gvfs-fuse",
                "engrampa", "ristretto", "mousepad", "menulibre", "mugshot"
            });
            if (!Executor::has("mugshot")) install_packages({"mugshot"});
            if (!fs::exists("/usr/share/themes/Breeze/xfwm4/themerc"))
                install_packages({"xfwm4-theme-breeze"});

            if (cfg_.sub_distro == "kali") {
                install_kali_undercover();
                PackageManager::install("kali-themes-common", DistroFamily::Debian);
                if (Executor::has("chromium"))
                    PackageManager::install("chromium-l10n", DistroFamily::Debian);
                Executor::shell("dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s "
                    "Windows-10-Icons 2>/dev/null || true");
            }

            // xfce4-panel-profiles (保留复杂 shell 下载逻辑)
            if (!Executor::has("xfce4-panel-profiles")) {
                if (cfg_.sub_distro == "ubuntu") {
                    auto os_release = SystemHelper::read_file("/etc/os-release");
                    bool is_bionic = (os_release.find("Bionic") != std::string::npos);
                    if (is_bionic)
                        PackageManager::install("xfpanel-switch", DistroFamily::Debian);
                    else
                        PackageManager::install("xfce4-panel-profiles", DistroFamily::Debian);
                } else {
                    // 非 Ubuntu 分支: 通过 apt 安装
                    PackageManager::install("xfce4-panel-profiles", DistroFamily::Debian);
                }
            }
        }
        if (is_ubuntu) get_ubuntu_desktop_language_pack();

        // qt5ct + QT_QPA_PLATFORMTHEME for ALL distros
        if (!Executor::has("qt5ct")) {
            install_packages({"qt5ct"});
        }
        if (Executor::has("qt5ct")) {
            if (!fs::exists("/etc/environment")) {
                SystemHelper::write_file("/etc/environment", "");
            }
            auto env_content = SystemHelper::read_file("/etc/environment");
            bool has_qt_platformtheme = false;
            std::istringstream env_stream(env_content);
            std::string env_line;
            while (std::getline(env_stream, env_line)) {
                // Skip leading whitespace, then skip comment lines
                size_t first_non_space = env_line.find_first_not_of(" \t");
                if (first_non_space != std::string::npos && env_line[first_non_space] == '#')
                    continue;
                if (env_line.find("QT_QPA_PLATFORMTHEME=") != std::string::npos) {
                    has_qt_platformtheme = true;
                    break;
                }
            }
            if (!has_qt_platformtheme)
                SystemHelper::append_file(fs::path("/etc/environment"), "export QT_QPA_PLATFORMTHEME=qt5ct\n");
        }

        if (is_debian || family == DistroFamily::Arch) {
            if (!fs::exists("/usr/share/icons/Breeze-Adapta-Cursor"))
                Executor::passthrough("wget -qO /tmp/breeze-adapta-cursor.tar.gz "
                    "https://github.com/arch-linux-archive/community/raw/master/breeze-adapta-cursor-theme.tar.gz 2>/dev/null && "
                    "tar -xzf /tmp/breeze-adapta-cursor.tar.gz -C /usr/share/icons/ 2>/dev/null || true");
        }

        // xfce4 config
        std::string xfce_conf = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root")
                                + "/.config/xfce4/xfconf/xfce-perchannel-xml/";
        CommandBuilder("mkdir").add_flag("-p").add_arg(xfce_conf).add_raw("2>/dev/null").execute();
        if (!fs::exists(xfce_conf + "xfce4-desktop.xml"))
            Executor::shell("cp -f /etc/xdg/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml "
                            + xfce_conf + " 2>/dev/null || true");
        if (!fs::exists(xfce_conf + "xfce4-panel.xml"))
            Executor::shell("cp -f /etc/xdg/xfce4/panel/default.xml "
                            + xfce_conf + "xfce4-panel.xml 2>/dev/null || true");

        if (cfg_.sub_distro != "kali") {
            std::string icon_theme = (family == DistroFamily::Alpine) ? "Faenza" : "Flat-Remix-Blue-Light";
            Executor::shell("dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s " + icon_theme +
                            " 2>/dev/null || true");
        }
        Executor::shell("dbus-launch xfconf-query -c xsettings -t string -np /Gtk/CursorThemeName -s "
            "\"Breeze-Adapta-Cursor\" 2>/dev/null || true");
        Executor::shell(
            "dbus-launch xfconf-query -c xfwm4 -t int -np /general/workspace_count -s 2 2>/dev/null || true");
        xfce4_color_scheme();
        Executor::shell(
            "dbus-launch xfconf-query -c xsettings -np /Net/ThemeName -s \"Adwaita-dark\" 2>/dev/null || "
            "xfconf-query -c xsettings -np /Net/ThemeName -s \"Greybird\" 2>/dev/null || true");
        if (is_proot) {
            remove_packages({
                "xfce4-power-manager", "xfce4-power-manager-data",
                "xfce4-power-manager-plugins", "xfce4-screensaver"
            });
        }
    }

    void DesktopManager::post_install_kde(DistroFamily family, bool is_debian, bool is_ubuntu, bool is_proot) {
        kde_warning();
        if (is_debian) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"kde-plasma or kde-standard\" --yes-button \"plasma\" --no-button \"standard\""
                                           " --yesno '前者为精简安装,后者为标准安装' 0 0");
            if (r.exit_code == 1) {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"kde-standard or kde-full\" --yes-button \"standard\" --no-button \"full\""
                                                " --yesno '前者包含KDE标准套件,后者为KDE全家桶' 0 0");
                if (r2.exit_code == 0) install_packages({"kde-standard"});
                else if (r2.exit_code == 1) install_packages({"kde-full"});
            }
            if (is_ubuntu) {
                auto r3 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"KDE-plasma or Kubuntu-desktop\" --yes-button \"KDE\" --no-button \"kubuntu\""
                                                " --yesno '前者为普通KDE,后者为kubuntu' 0 0");
                if (r3.exit_code == 1) install_packages({"kubuntu-desktop"});
            }
        }
        if (family == DistroFamily::Arch) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"kde-plasma or kde-standard\" --yes-button \"plasma\" --no-button \"plasma+apps\""
                                           " --yesno '前者为精简安装,后者包含kde-applications' 0 0");
            if (r.exit_code == 1) PackageManager::install("kde-applications", DistroFamily::Arch);
        }
        if (is_proot) {
            remove_packages({
                "plasma-powerdevil", "plasma-discover",
                "plasma-discover-backend-flatpak", "plasma-discover-backend-snap"
            });
        }
        if (is_debian) {
            auto wayland_r = Executor::passthrough(cfg_.tui_bin +
                                                   " --title \"x11 or wayland\" --yes-button \"x11\" --no-button \"wayland\""
                                                   " --yesno '默认推荐x11, wayland尚在实验阶段' 0 0");
            if (wayland_r.exit_code == 1) {
                plasma_wayland_env();
                Logger::info(_("gui.plasma_wayland.selected"));
            }
        }
    }

    void DesktopManager::post_install_gnome(DistroFamily family, bool is_debian, bool is_ubuntu) {
        gnome3_warning();
        print_gnome_ascii();
        if (is_ubuntu) get_ubuntu_desktop_language_pack();
        if (is_debian) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"gnome-shell or gnome-core\" --yes-button \"gnome-shell\" --no-button \"gnome-core\""
                                           " --yesno '前者更精简,后者包含额外组件' 0 0");
            if (r.exit_code == 1) install_packages({"gnome-core"});
            if (is_ubuntu) {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"gnome or ubuntu-desktop\" --yes-button \"gnome\" --no-button \"ubuntu-desktop\""
                                                " --yesno '前者为gnome基础桌面,后者为ubuntu-desktop' 0 0");
                if (r2.exit_code == 1) install_packages({"ubuntu-desktop"});
            }
            std::string session_menu = cfg_.tui_bin +
                                       " --title \"gnome-session\""
                                       " --menu \"请选择 GNOME 会话类型\\nWhich gnome session do you prefer?\" 0 0 0 "
                                       "\"1\" \"gnome-shell-x11 (推荐)\" "
                                       "\"2\" \"gnome-flashback (经典Flashback)\" "
                                       "\"3\" \"gnome-session (标准)\" "
                                       "\"4\" \"gnome-session-ubuntu (Ubuntu风格)\" "
                                       "\"5\" \"gnome-session-classic (经典风格)\" "
                                       "\"0\" \"Back\"";
            auto session_ch = Executor::tui_select(session_menu);
            if (session_ch == "1") {
                SystemHelper::write_file("/usr/local/bin/gnome-shell-x11", generate_gnome_shell_x11());
                CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-shell-x11").execute();
            } else if (session_ch == "2") {
                SystemHelper::write_file("/usr/local/bin/gnome-flashback-metacity",
                                         generate_gnome_flashback_metacity());
                CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-flashback-metacity").execute();
            } else if (session_ch == "3") {
                /* uses default gnome-session */
            } else if (session_ch == "4") {
                SystemHelper::write_file("/usr/local/bin/gnome-session-ubuntu", generate_gnome_session_ubuntu());
                CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-session-ubuntu").execute();
            } else if (session_ch == "5") {
                SystemHelper::write_file("/usr/local/bin/gnome-session-classic", generate_gnome_session_classic());
                CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-session-classic").execute();
            }
        }
        if (family == DistroFamily::Arch) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"gnome or gnome-extra\" --yes-button \"gnome\" --no-button \"gnome-extra\""
                                           " --yesno '前者为gnome基础包,后者包含gnome-extra' 0 0");
            if (r.exit_code == 1) PackageManager::install("gnome-extra", DistroFamily::Arch);
        }
        if (family == DistroFamily::RedHat)
            Executor::passthrough(cfg_.install_command + " @'GNOME Desktop' 2>/dev/null || true");

        set_gnome_common_env();
        ln_s_gnome_flashback_metacity();
    }

    void DesktopManager::post_install_mate(DistroFamily family, bool is_debian, bool is_ubuntu) {
        if (family == DistroFamily::Arch && cfg_.is_termux) arch_linux_mate_warning();
        if (is_debian) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"MATE-CORE or MATE-LITE\" --yes-button \"core\" --no-button \"lite\""
                                           " --yesno '前者为普通mate,后者为精简版mate' 0 0");
            if (r.exit_code == 0) install_packages({"mate-desktop-environment"});
            else install_packages({"mate-desktop-environment-core"});
            if (is_ubuntu) {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"Mate or Ubuntu-MATE\" --yes-button \"mate\" --no-button \"ubuntu-mate\""
                                                " --yesno '前者为普通mate,后者为ubuntu-mate' 0 0");
                if (r2.exit_code == 1) install_packages({"ubuntu-mate-desktop"});
            }
            Executor::passthrough("apt clean 2>/dev/null || true");
            Executor::passthrough("apt autoclean 2>/dev/null || true");
        }
    }

    void DesktopManager::post_install_lxqt(bool is_debian, bool is_ubuntu) {
        if (is_debian) {
            Executor::passthrough(cfg_.tui_bin +
                                  " --title \"LXQT-CORE or LXQT-LITE\" --yes-button \"core\" --no-button \"lite\""
                                  " --yesno '前者为普通lxqt,后者为精简版lxqt' 0 0");
            if (is_ubuntu) {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"Lxqt or Lubuntu-desktop\" --yes-button \"lxqt\" --no-button \"lubuntu\""
                                                " --yesno '前者为普通lxqt,后者为lubuntu' 0 0");
                if (r2.exit_code == 1) install_packages({"lubuntu-desktop"});
            }
        }
    }

    void DesktopManager::post_install_lxde(bool is_debian) {
        if (is_debian) {
            Executor::passthrough(cfg_.tui_bin +
                                  " --title \"LXDE-CORE or LXDE-LITE\" --yes-button \"core\" --no-button \"lite\""
                                  " --yesno '前者包含额外组件,后者更精简' 0 0");
        }
    }

    void DesktopManager::post_install_cinnamon(DistroFamily family, bool is_debian) {
        cinnamon_warning();
        if (is_debian) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"Lite or standard\" --yes-button \"lite\" --no-button \"standard\""
                                           " --yesno '前者更精简,后者包含额外组件\\nThe former is more streamlined' 0 0");
            if (r.exit_code == 0)
                Executor::passthrough(
                    cfg_.install_command + " --no-install-recommends cinnamon-l10n cinnamon 2>/dev/null || true");
            else
                install_packages({"cinnamon-l10n", "cinnamon-desktop-environment", "cinnamon"});
            auto issue_content = SystemHelper::read_file("/etc/issue");
            if (issue_content.find("Linux Mint") != std::string::npos)
                install_packages({"mint-meta-cinnamon", "mint-meta-core", "mint-artwork"});
            Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
        } else if (family == DistroFamily::RedHat) {
            Executor::passthrough(cfg_.install_command + " @'Cinnamon Desktop' 2>/dev/null || true");
        } else if (family == DistroFamily::Arch) {
            auto pkgs = std::vector<std::string>{"cinnamon-translations", "cinnamon"};
            PackageManager::install(pkgs, DistroFamily::Arch);
        } else if (family == DistroFamily::Gentoo) {
            Executor::passthrough(
                "emerge -av gnome-extra/cinnamon gnome-extra/cinnamon-desktop gnome-extra/cinnamon-translations 2>/dev/null || true");
        } else if (family == DistroFamily::Suse) {
            auto pkgs = std::vector<std::string>{"cinnamon", "cinnamon-control-center"};
            PackageManager::install(pkgs, DistroFamily::Suse);
        } else if (family == DistroFamily::Alpine) {
            PackageManager::install("adapta-cinnamon", DistroFamily::Alpine);
        }
    }

    void DesktopManager::post_install_budgie(bool is_debian, bool is_ubuntu) {
        tmoe_desktop_warning();
        if (is_debian) {
            Executor::passthrough(cfg_.tui_bin +
                                  " --title \"budgie-core or budgie-desktop\" --yes-button \"core\" --no-button \"desktop\""
                                  " --yesno '前者更精简,后者包含额外组件' 0 0");
            if (is_ubuntu) {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"budgie or ubuntu-budgie-desktop\" --yes-button \"budgie\" --no-button \"ubuntu-budgie\""
                                                " --yesno '前者为budgie基础桌面,后者为ubuntu-budgie-desktop' 0 0");
                if (r2.exit_code == 1) install_packages({"ubuntu-budgie-desktop"});
            }
            auto panel_r = Executor::passthrough(cfg_.tui_bin +
                                                 " --title \"budgie session\" --yes-button \"panel\" --no-button \"desktop\""
                                                 " --yesno 'budgie-panel + budgie-wm or budgie-desktop?' 0 0");
            if (panel_r.exit_code == 0) set_budgie_desktop_session("panel");
        }
        SystemHelper::write_file("/usr/local/bin/budgie-desktop-builtin", generate_budgie_desktop_builtin());
        CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/budgie-desktop-builtin").execute();
    }

    void DesktopManager::post_install_dde_or_deepin(DistroFamily family, bool is_debian) {
        deepin_desktop_warning();
        if (is_debian) {
            if (cfg_.sub_distro == "deepin") {
                install_packages({"dde"});
            } else {
                deepin_desktop_debian();
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"DDE or DDE-extras\" --yes-button \"dde\" --no-button \"dde-extras\""
                                               " --yesno '前者更精简,后者包含dde额外软件包' 0 0");
                if (r.exit_code == 1)
                    install_packages({"ubuntudde-dde-extras"});
                else
                    install_packages({"ubuntudde-dde", "deepin-terminal"});
            }
            Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
            Executor::passthrough("apt clean 2>/dev/null || true"); {
                const char *postinst_files[] = {
                    "/var/lib/dpkg/info/mincores-dkms.postinst",
                    "/var/lib/dpkg/info/warm-sched.postinst"
                };
                for (const auto &pf: postinst_files) {
                    if (fs::exists(pf)) {
                        auto pf_content = SystemHelper::read_file(pf);
                        size_t newline_pos = pf_content.find('\n');
                        if (newline_pos != std::string::npos) {
                            pf_content.insert(newline_pos + 1, "return 0\n");
                            SystemHelper::write_file(pf, pf_content);
                        }
                    }
                }
                Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
            }
        } else if (family == DistroFamily::RedHat) {
            PackageManager::install("deepin-desktop", DistroFamily::RedHat);
        } else if (family == DistroFamily::Arch) {
        Logger::warn(_("gui.deepin.clutter_conflict"));
        Logger::info(_("gui.deepin.clutter_fix"));
            PackageManager::install({"deepin", "xorg", "deepin-extra", "lightdm", "lightdm-deepin-greeter"},
                                    DistroFamily::Arch);
        }
    }

    void DesktopManager::post_install_ukui(bool is_ubuntu) {
        if (is_ubuntu) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"ukui or ubuntukylin-desktop\" --yes-button \"ukui\" --no-button \"kylin\""
                                           " --yesno '前者为普通ukui,后者为ubuntukylin-desktop' 0 0");
            if (r.exit_code == 1) install_packages({"ubuntukylin-desktop"});
        }
    }

    void DesktopManager::post_install_cutefish(DistroFamily family, bool is_debian, bool is_ubuntu, bool is_proot) {
        Logger::info(_("gui.cutefish.intro"));
        if (is_proot) {
        Logger::warn(_("gui.cutefish.systemd_warn"));
        }
        if (is_debian) {
            // Cutefish 在 Debian/Ubuntu 上需要从社区源安装
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"Cutefish install type\" --yes-button \"core\" --no-button \"full\""
                                           " --yesno '前者为 cutefish 核心桌面, 后者包含额外组件' 0 0");
            if (r.exit_code == 0) {
                install_packages({"cutefish", "cutefish-core"});
            } else {
                install_packages({
                    "cutefish", "cutefish-core", "cutefish-settings",
                    "cutefish-dock", "cutefish-launcher", "cutefish-filemanager",
                    "cutefish-terminal", "cutefish-texteditor"
                });
            }
        } else if (family == DistroFamily::Arch) {
            PackageManager::install({
                                        "cutefish", "cutefish-core", "cutefish-settings",
                                        "cutefish-dock", "cutefish-launcher", "cutefish-filemanager",
                                        "cutefish-terminal"
                                    },
                                    DistroFamily::Arch);
        } else if (family == DistroFamily::RedHat) {
            Executor::passthrough(cfg_.install_command + " cutefish cutefish-core 2>/dev/null || true");
        } else {
            install_packages({"cutefish"});
        }
    }


    bool DesktopManager::install_window_manager(std::string_view wm) {
        // 统一走 install_desktop (DesktopInfo 已包含所有 WM 条目)
        return install_desktop(wm);
    }


    std::vector<std::string> DesktopManager::list_window_managers() const {
        std::vector<std::string> names;
        for (const auto &info: desktop_registry()) {
            if (info.is_window_manager) names.emplace_back(info.id);
        }
        return names;
    }

    // ═══════════════════════════════════════════════════════════════
    // noVNC (HTML5 VNC 客户端)
    // ═══════════════════════════════════════════════════════════════


    void DesktopManager::after_desktop_install_hint() const {
        Logger::info(_("gui.after_install.hint"));
    }


    void DesktopManager::download_iosevka_ttf_font_ext() {
        // 对应旧 Bash download_iosevka_ttf_font (gui:388-444)
        const char *iosevka_file = "/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf";
        if (fs::exists(iosevka_file)) return;

        Logger::info(_("gui.installing_iosevka"));
        CommandBuilder("mkdir").add_flag("-pv").add_arg("/usr/share/fonts/truetype/iosevka/").add_raw("2>/dev/null").execute();

        // 检查 /tmp/font.ttf 是否已存在且 sha256 匹配
        auto sha_check = Executor::shell("cd /tmp && [ -e font.ttf ] && sha256sum font.ttf 2>/dev/null");
        std::string expected_sha = "cb4f09f9ec1b0d21021dce6c6dbe4f7ecb4930cbea0c766da1fe478111a5844e";
        if (sha_check.ok() && sha_check.stdout_data.find(expected_sha) != std::string::npos) {
            CommandBuilder("cp").add_flag("-fv").add_arg("/tmp/font.ttf").add_arg(iosevka_file).execute();
            return;
        } else if (fs::exists("/tmp/font.ttf")) {
            Executor::shell("mv -vf /tmp/font.ttf /usr/share/fonts/truetype/iosevka/Iosevka.ttf 2>/dev/null || true");
        }

        // 确定字体缓存目录
        std::string font_dir;
        if (fs::exists("/etc/gitstatus")) {
            if (fs::exists("/root/.cache/gitstatus")) {
                Executor::shell("cp -f /root/.cache/gitstatus/* /etc/gitstatus 2>/dev/null || true");
                CommandBuilder("chmod").add_flag("-R").add_arg("a+rx").add_arg("/etc/gitstatus/").add_raw("2>/dev/null || true").execute();
            }
            font_dir = "/etc/gitstatus";
        } else {
            font_dir = "/root/.cache/gitstatus";
            CommandBuilder("mkdir").add_flag("-pv").add_arg(font_dir).add_raw("2>/dev/null").execute();
        }

        // 从缓存解压
        if (fs::exists(font_dir + "/Iosevka-Term-Mono.tar.xz")) {
            Executor::shell("cd " + font_dir + " && tar -Jxvf Iosevka-Term-Mono.tar.xz 2>/dev/null && "
                            "mv -vf Iosevka.ttf " + std::string(iosevka_file) + " 2>/dev/null || true");
        }

        // 下载字体
        if (!fs::exists(iosevka_file)) {
            std::string font_url_01 = "https://gitee.com/ak2/inconsolata-go-font/raw/master/Iosevka-Term-Mono.tar.xz";
            std::string font_url_02 =
                    "https://github.com/cu233/Iosevka-Term-Mono-Font/raw/master/Iosevka-Term-Mono.tar.xz";
            Executor::shell("cd " + font_dir + " && "
                            "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -o Iosevka-Term-Mono.tar.xz "
                            + font_url_02 + " 2>/dev/null || "
                            "curl -Lo 'Iosevka-Term-Mono.tar.xz' " + font_url_01 + " 2>/dev/null) && "
                            "tar -Jxvf 'Iosevka-Term-Mono.tar.xz' 2>/dev/null && "
                            "mv -vf Iosevka.ttf " + std::string(iosevka_file) + " 2>/dev/null || true");
        }

        // 二次检查
        if (!fs::exists(iosevka_file)) {
            Executor::shell("rm -fv " + font_dir + "/Iosevka-Term-Mono.tar.xz 2>/dev/null || true");
        }

        // 刷新字体缓存
        Executor::shell("cd /usr/share/fonts/truetype/iosevka/ && "
            "mkfontscale 2>/dev/null; mkfontdir 2>/dev/null; fc-cache 2>/dev/null || true");
    }


    bool DesktopManager::install_fonts() {
        Logger::step(_("gui.font.install"));

        auto family = resolved_family();
        std::vector<std::string> font_pkgs;
        switch (family) {
            case DistroFamily::Arch:
                font_pkgs = {"noto-fonts-cjk", "noto-fonts-emoji"};
                break;
            case DistroFamily::Debian:
            default:
                font_pkgs = {"fonts-noto-cjk", "fonts-noto-color-emoji"};
                break;
        }
        install_packages(font_pkgs);

        // 刷新字体缓存
        Executor::shell("fc-cache -fv 2>/dev/null || true");
        Logger::ok(_("gui.font.install_ok"));
        return true;
    }


    bool DesktopManager::install_fcitx() {
        Logger::step(_("gui.fcitx.install"));

        // fcitx + 拼音 + 配置工具
        std::vector<std::string> pkgs = {
            "fcitx", "fcitx-pinyin", "fcitx-config-gtk", "fcitx-frontend-gtk2",
            "fcitx-frontend-gtk3", "fcitx-frontend-qt5"
        };

        if (!install_packages(pkgs)) {
        Logger::warn(_("gui.fcitx.partial_fail"));
        }

        // 配置环境变量
        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        std::string bashrc = home + "/.bashrc";
        std::string env_block =
                "\n# tmoe fcitx (已自动配置)\n"
                "export GTK_IM_MODULE=fcitx\n"
                "export QT_IM_MODULE=fcitx\n"
                "export XMODIFIERS=@im=fcitx\n"
                "export DefaultIMModule=fcitx\n";
        SystemHelper::append_file(fs::path(bashrc), env_block);

        // 创建 fcitx 自启动
        std::string autostart_dir = home + "/.config/autostart";
        CommandBuilder("mkdir").add_flag("-p").add_arg(autostart_dir).execute();
        std::string fcitx_desktop = autostart_dir + "/fcitx.desktop";
        std::string desktop_content =
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Fcitx\n"
                "Exec=fcitx-autostart\n"
                "X-GNOME-Autostart-enabled=true\n";
        SystemHelper::write_file(fs::path(fcitx_desktop), desktop_content);

        Logger::ok(_("gui.fcitx.install_ok"));
        return true;
    }

    void DesktopManager::set_gnome_common_env() {
        // 设置 GNOME 公共环境变量
        Executor::shell("gsettings set org.gnome.desktop.interface gtk-theme 'Adwaita' 2>/dev/null || true");
        Executor::shell("gsettings set org.gnome.desktop.interface icon-theme 'Adwaita' 2>/dev/null || true");
    }


    void DesktopManager::ln_s_gnome_flashback_metacity() {
        // 检查是否需要为 gnome-flashback 创建软链接
        if (fs::exists("/usr/lib/gnome-flashback/gnome-flashback-metacity")) {
            Executor::shell("ln -svf /usr/lib/gnome-flashback/gnome-flashback-metacity "
                "/usr/local/bin/gnome-flashback-metacity 2>/dev/null || true");
            auto gfbm_content = SystemHelper::read_file("/usr/local/bin/gnome-flashback-metacity");
            if (gfbm_content.find("--systemd") != std::string::npos) {
                Executor::shell("rm -vf /usr/local/bin/gnome-flashback-metacity 2>/dev/null || true");
                SystemHelper::write_file("/usr/local/bin/gnome-flashback-metacity",
                                         generate_gnome_flashback_metacity());
                CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-flashback-metacity").execute();
            }
        } else {
            SystemHelper::write_file("/usr/local/bin/gnome-flashback-metacity", generate_gnome_flashback_metacity());
            CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/gnome-flashback-metacity").execute();
        }
    }


    void DesktopManager::set_gnome_desktop_deps() {
        auto family = resolved_family();
        std::string dep;
        switch (family) {
            case DistroFamily::Debian: dep = "gnome-shell gnome-session";
                break;
            case DistroFamily::RedHat: dep = "@'GNOME Desktop'";
                break;
            case DistroFamily::Arch: dep = "gnome gnome-shell gnome-session";
                break;
            case DistroFamily::Gentoo: dep = "gnome-base/gnome gnome-base/gnome-shell";
                break;
            case DistroFamily::Suse: dep = "gnome-session gnome-shell";
                break;
            case DistroFamily::Alpine: dep = "gnome gnome-shell";
                break;
            default: dep = "gnome-session gnome-shell";
                break;
        }
        Executor::passthrough(cfg_.install_command + " " + dep + " 2>/dev/null || true");
    }


    void DesktopManager::get_ubuntu_desktop_language_pack() {
        if (cfg_.sub_distro != "ubuntu") return;
        auto lang = std::string(I18n::current_lang());
        if (lang.find("zh") == 0) {
            PackageManager::install({"language-pack-zh-hans", "language-pack-gnome-zh-hans"},
                                    DistroFamily::Debian);
        }
    }


    void DesktopManager::set_budgie_desktop_session(const std::string &session_type) {
        if (session_type == "panel") {
            SystemHelper::write_file("/usr/local/bin/budgie-desktop-builtin", generate_budgie_desktop_builtin());
            CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/budgie-desktop-builtin").execute();
        }
    }


    void DesktopManager::touch_xfce4_terminal_rc_ext() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string term_dir = home + "/.config/xfce4/terminal";
        CommandBuilder("mkdir").add_flag("-p").add_arg(term_dir).add_raw("2>/dev/null").execute();
        if (!fs::exists(term_dir + "/terminalrc")) {
            SystemHelper::write_file(fs::path(term_dir + "/terminalrc"), generate_xfce_terminal_rc());
        }
    }


    void DesktopManager::xfce4_color_scheme() {
        // 对应旧 Bash xfce4_color_scheme (gui:1379-1429)
        if (!fs::exists("/usr/share/xfce4/terminal/colorschemes/Monokai Remastered.theme")) {
        Logger::info(_("gui.xfce4.color_scheme_config"));
            Executor::shell("cd /usr/share/xfce4/terminal && "
                "curl -Lo 'colorschemes.tar.xz' "
                "'https://gitee.com/mo2/xfce-themes/raw/terminal/colorschemes.tar.xz' 2>/dev/null && "
                "tar -Jxvf 'colorschemes.tar.xz' 2>/dev/null || true");
        }

        touch_xfce4_terminal_rc_ext();


        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string term_dir = home + "/.config/xfce4/terminal";
        std::string termrc = term_dir + "/terminalrc";

        // 确保有 ColorPalette
        auto termrc_content = SystemHelper::read_file(termrc);
        bool has_color_palette = false; {
            std::istringstream cp_stream(termrc_content);
            std::string cp_line;
            while (std::getline(cp_stream, cp_line)) {
                size_t cp_ns = cp_line.find_first_not_of(" \t");
                if (cp_ns != std::string::npos && cp_line.compare(cp_ns, 12, "ColorPalette") == 0) {
                    has_color_palette = true;
                    break;
                }
            }
        }
        if (!has_color_palette) {
            // Remove any existing ColorPalette=/ColorForeground=/ColorBackground= lines
            std::istringstream iss(termrc_content);
            std::string line;
            std::string filtered;
            while (std::getline(iss, line)) {
                if (line.find("ColorPalette=") == std::string::npos &&
                    line.find("ColorForeground=") == std::string::npos &&
                    line.find("ColorBackground=") == std::string::npos) {
                    filtered += line + "\n";
                }
            }
            SystemHelper::write_file(fs::path(termrc), filtered);
            SystemHelper::append_file(fs::path(termrc),
                                      "ColorPalette=#000000;#ff3333;#b8cc52;#e7c547;#36a3d9;#f07178;#95e6cb;#ffffff;"
                                      "#323232;#ff6565;#eafe84;#fff779;#68d5ff;#ffa3aa;#c7fffd;#ffffff\n"
                                      "ColorForeground=#e6e1cf\n"
                                      "ColorBackground=#0f1419\n");
        }

        // 设置字体
        auto termrc_final = SystemHelper::read_file(termrc);
        bool has_font_name = false; {
            std::istringstream fn_stream(termrc_final);
            std::string fn_line;
            while (std::getline(fn_stream, fn_line)) {
                size_t fn_ns = fn_line.find_first_not_of(" \t");
                if (fn_ns != std::string::npos && fn_line.compare(fn_ns, 8, "FontName") == 0) {
                    has_font_name = true;
                    break;
                }
            }
        }
        if (!has_font_name) {
            if (fs::exists("/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf")) {
                SystemHelper::append_file(fs::path(termrc), "FontName=Iosevka Term Bold 12\n");
            } else if (fs::exists("/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc")) {
                SystemHelper::append_file(fs::path(termrc), "FontName=Noto Sans Mono CJK SC Bold 12\n");
            }
        }
    }

    void DesktopManager::install_kali_undercover() {
        if (fs::exists("/usr/share/icons/Windows-10-Icons")) {
        Logger::info(_("gui.kali.undercover_exists"));
            return;
        }
        Logger::step(_("gui.kali.undercover_install"));
        Executor::shell("mkdir -pv /tmp/.kali-undercover-win10-theme && cd /tmp/.kali-undercover-win10-theme");
        std::string repo = "https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-undercover";
        Executor::shell("cd /tmp/.kali-undercover-win10-theme && "
                        "UNDERCOVERlatestLINK=\"$(curl -L '" + repo + "/' 2>/dev/null | grep all.deb | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2)\" && "
                        "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o kali-undercover.deb '" + repo + "/'\"$UNDERCOVERlatestLINK\" 2>/dev/null || "
                        "apt download kali-undercover 2>/dev/null && mv *deb kali-undercover.deb) && "
                        "ar xv kali-undercover.deb && cd / && "
                        "tar -Jxvf /tmp/.kali-undercover-win10-theme/data.tar.xz ./usr 2>/dev/null && "
                        "mv -f /usr/bin/kali-undercover /usr/local/bin/ 2>/dev/null; "
                        "update-icon-caches /usr/share/icons/Windows-10-Icons 2>/dev/null &");
        Executor::shell("rm -rfv /tmp/.kali-undercover-win10-theme 2>/dev/null || true");
        Logger::ok(_("gui.kali.undercover_ok"));
    }


    void DesktopManager::download_macos_mojave_theme() {
        Logger::step(_("gui.macos_mojave.install"));
        if (fs::exists("/usr/share/themes/Mojave-dark")) {
            Logger::info(_("gui.theme.already_installed"));
            return;
        }
        Executor::shell("cd /tmp && [ -d McMojave ] && rm -rf McMojave; "
            "git clone -b McMojave --depth=1 https://gitee.com/mo2/xfce-themes.git /tmp/McMojave 2>/dev/null && "
            "cd /tmp/McMojave && "
            "tar -Jxvf 01-Mojave-dark.tar.xz -C /usr/share/themes 2>/dev/null && "
            "tar -Jxvf 01-McMojave-circle.tar.xz -C /usr/share/icons 2>/dev/null && "
            "update-icon-caches /usr/share/icons/McMojave-circle-dark /usr/share/icons/McMojave-circle 2>/dev/null &");
        Executor::shell("rm -rf /tmp/McMojave 2>/dev/null || true");
        set_default_xfce_icon_theme("McMojave-circle");
        Logger::ok(_("gui.macos_mojave.ok"));
    }


    void DesktopManager::download_macos_bigsur_theme() {
        Logger::step(_("gui.macos_bigsur.install"));
        if (fs::exists("/usr/share/icons/WhiteSur-dark")) {
            Logger::info(_("gui.theme.already_installed"));
            return;
        }
        Executor::shell("cd /tmp && [ -d BIGSUR_TEMP_FOLDER ] && rm -rvf BIGSUR_TEMP_FOLDER; "
            "git clone -b master --depth=1 https://gitee.com/ak2/bigsur-gtk-theme.git "
            "/tmp/BIGSUR_TEMP_FOLDER 2>/dev/null && "
            "cd /tmp/BIGSUR_TEMP_FOLDER && "
            "tar -Jxvf WhiteSur.tar.xz -C /usr/share/icons 2>/dev/null && "
            "tar -Jxvf WhiteSur-light-alt.tar.xz -C /usr/share/themes 2>/dev/null && "
            "tar -Jxvf WhiteSur-dark.tar.xz -C /usr/share/themes 2>/dev/null && "
            "update-icon-caches /usr/share/icons/WhiteSur /usr/share/icons/WhiteSur-dark 2>/dev/null &");
        Executor::shell("rm -rvf /tmp/BIGSUR_TEMP_FOLDER 2>/dev/null || true");
        set_default_xfce_icon_theme("WhiteSur");
        Logger::ok(_("gui.macos_bigsur.ok"));
    }


    void DesktopManager::install_breeze_theme_ext() {
        Logger::step(_("gui.breeze.install"));
        download_arch_breeze_adapta_cursor_theme();
        auto family = resolved_family();
        if (family == DistroFamily::Arch) {
            PackageManager::install({"breeze-icons", "breeze-gtk", "xfwm4-theme-breeze", "capitaine-cursors"},
                                    DistroFamily::Arch);
        } else {
            Executor::passthrough(
                cfg_.install_command +
                " breeze-icon-theme breeze-cursor-theme breeze-gtk-theme xfwm4-theme-breeze 2>/dev/null || true");
        }
        Logger::ok(_("gui.breeze.ok"));
    }


    void DesktopManager::install_arc_gtk_theme_ext() {
        Executor::passthrough(cfg_.install_command + " arc-icon-theme 2>/dev/null || true");
        auto family = resolved_family();
        std::string arc_pkg = (family == DistroFamily::Arch) ? "arc-gtk-theme" : "arc-theme";
        Executor::passthrough(cfg_.install_command + " " + arc_pkg + " 2>/dev/null || true");
    }


    void DesktopManager::set_default_xfce_icon_theme(const std::string &icon_name) {
        Executor::shell(
            "dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s " + icon_name + " 2>/dev/null || true");
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        if (home != "/root") {
            std::string user = Executor::shell("id -un").stdout_data;
            std::string group = Executor::shell("id -gn").stdout_data;
            while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
            while (!group.empty() && (group.back() == '\n' || group.back() == '\r')) group.pop_back();
            CommandBuilder("chown").add_flag("-Rv").add_arg(user + ":" + group).add_arg(home + "/.config/xfce4").add_raw("2>/dev/null || true").execute();
        }
    }


    void DesktopManager::create_update_icon_caches() {
        SystemHelper::write_file("/usr/local/bin/update-icon-caches", generate_update_icon_caches_script());
        CommandBuilder("chmod").add_arg("a+rx").add_arg("/usr/local/bin/update-icon-caches").execute();
    }


    void DesktopManager::check_update_icon_caches_sh() {
        if (!Executor::has("update-icon-caches")) create_update_icon_caches();
    }


    void DesktopManager::git_clone_kali_themes_common() {
        if (fs::exists("/usr/share/desktop-base/kali-theme")) return;
        check_update_icon_caches_sh();
        Logger::step(_("gui.kali.clone_theme"));
        Executor::shell(
            "TEMP_FOLDER=/tmp/.KALI_THEME_COMMON_TEMP_FOLDER && [ -d \"$TEMP_FOLDER\" ] && rm -rvf \"$TEMP_FOLDER\"; "
            "git clone --depth=1 https://gitee.com/ak2/kali-theme.git \"$TEMP_FOLDER\" 2>/dev/null && "
            "cd \"$TEMP_FOLDER\" && tar -pJxvf kali-theme.tar.xz -C / 2>/dev/null && "
            "rm -rvf \"$TEMP_FOLDER\" && "
            "dbus-launch xfconf-query -c xsettings -t string -np /Gtk/CursorThemeName -s "
            "\"Breeze-Adapta-Cursor\" 2>/dev/null && "
            "update-icon-caches /usr/share/icons/Flat-Remix-Blue-Dark /usr/share/icons/Flat-Remix-Blue-Light "
            "/usr/share/icons/desktop-base 2>/dev/null &");
    }


    void DesktopManager::download_kali_themes_common() {
        check_update_icon_caches_sh();
        std::string repo = "https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-themes/";
        std::string repo_02 = "http://http.kali.org/kali/pool/main/k/kali-themes/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'kali-themes-common' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o kali-themes.deb '" + repo + "'\"$LATEST\" 2>/dev/null || "
                        "curl -L -o kali-themes.deb '" + repo_02 + "'\"$LATEST\" 2>/dev/null; "
                        "[ -f kali-themes.deb ] && (ar xv kali-themes.deb && tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
        Executor::shell("update-icon-caches /usr/share/icons/Flat-Remix-Blue-Dark "
            "/usr/share/icons/Flat-Remix-Blue-Light /usr/share/icons/desktop-base 2>/dev/null &");
    }


    void DesktopManager::download_kali_theme() {
        if (!fs::exists("/usr/share/desktop-base/kali-theme")) {
            download_kali_themes_common();
        } else {
        Logger::info(_("gui.kali.themes_downloaded"));
            download_kali_themes_common();
        }
        set_default_xfce_icon_theme("Flat-Remix-Blue-Light");
    }


    void DesktopManager::download_arch_breeze_adapta_cursor_theme() {
        if (fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) return;
        Logger::step(_("gui.breeze.cursor_download"));
        Executor::shell("mkdir -pv /tmp/.breeze_theme && cd /tmp/.breeze_theme && "
            "THEME_URL='https://mirrors.bfsu.edu.cn/archlinuxcn/any/' && "
            "curl -Lo index.html \"$THEME_URL\" 2>/dev/null && "
            "GREP_NAME='breeze-adapta-cursor-theme-git' && "
            "LATEST=$(cat index.html | grep \"$GREP_NAME\" | grep '.pkg.tar.zst' | tail -n 1 | "
            "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
            "[ -n \"$LATEST\" ] && curl -Lo data.tar.zst \"$THEME_URL$LATEST\" 2>/dev/null && "
            "tar --use-compress-program zstd -xvf data.tar.zst 2>/dev/null && "
            "cp -rf usr / 2>/dev/null || true");
        Executor::shell("rm -rf /tmp/.breeze_theme 2>/dev/null || true");
    }


    void DesktopManager::install_moka_theme_ext() {
        Executor::passthrough(cfg_.install_command + " moka-icon-theme 2>/dev/null || true");
    }


    void DesktopManager::install_numix_theme_ext() {
        Executor::passthrough(cfg_.install_command + " numix-gtk-theme 2>/dev/null || true");
        auto family = resolved_family();
        std::string circle_pkg = (family == DistroFamily::Arch)
                                     ? "numix-circle-icon-theme-git"
                                     : "numix-icon-theme-circle";
        Executor::passthrough(cfg_.install_command + " " + circle_pkg + " 2>/dev/null || true");
    }


    void DesktopManager::xfce_warning() const {
        Logger::info(_("gui.xfce4.warning"));
    }


    void DesktopManager::kde_warning() const {
        Logger::info(_("gui.kde.warning"));
    }


    void DesktopManager::gnome3_warning() const {
        Logger::info(_("gui.gnome.warning"));
    }


    void DesktopManager::cinnamon_warning() const {
        auto family = resolved_family();
        if (cfg_.is_termux) {
            Logger::warn(_("gui.cinnamon.proot_warn"));
        }
    }


    void DesktopManager::deepin_desktop_warning() const {
        Logger::info(_("gui.deepin.warning"));
        if (cfg_.is_termux) {
            Logger::warn(_("gui.deepin.proot_warn"));
        }
    }


    void DesktopManager::arch_linux_mate_warning() const {
        Logger::warn(_("gui.mate.warning"));
    }


    void DesktopManager::tmoe_desktop_warning() const {
        Logger::warn(_("gui.proot.warning"));
    }


    void DesktopManager::tips_of_tiger_vnc_server() const {
        Logger::info(_("gui.tiger_vnc.tips"));
    }


    void DesktopManager::tmoe_desktop_faq() const {
        // 对应旧 Bash tmoe_desktop_faq: 显示 tmoe 桌面 FAQ (source 旧 faq 文件内容)
        Logger::info(_("gui.faq.content"));
    }


    void DesktopManager::ubuntu_dde_distro_code(std::string &target_code) {
        Executor::shell("cd /tmp && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
            "-o .ubuntu_ppa_tmoe_cache "
            "'http://ppa.launchpad.net/ubuntudde-dev/stable/ubuntu/dists/' 2>/dev/null || true");
        auto r = Executor::shell("cat /tmp/.ubuntu_ppa_tmoe_cache 2>/dev/null | grep '\\[DIR' | tail -n 1 | "
            "cut -d '=' -f 5 | cut -d '/' -f 1 | cut -d '\"' -f 2");
        if (r.ok() && !r.stdout_data.empty()) {
            target_code = r.stdout_data;
            while (!target_code.empty() && (target_code.back() == '\n' || target_code.back() == '\r'))
                target_code.pop_back();
        }
        Executor::shell("rm -f /tmp/.ubuntu_ppa_tmoe_cache 2>/dev/null || true");
    }


    void DesktopManager::deepin_desktop_debian() {
        if (!Executor::has("add-apt-repository")) {
            PackageManager::update(DistroFamily::Debian);
            PackageManager::install({"software-properties-common", "gnupg"}, DistroFamily::Debian);
        }
        Executor::passthrough("yes | add-apt-repository ppa:ubuntudde-dev/stable 2>/dev/null || true");
        if (cfg_.sub_distro != "ubuntu") {
            std::string target_code;
            ubuntu_dde_distro_code(target_code);
            // 修复 PPA list 中的发行版代码
            if (!target_code.empty()) {
                Executor::shell(
                    "PPA_LIST_FILE=$(ls /etc/apt/sources.list.d/ubuntudde-dev-ubuntu-stable-*.list 2>/dev/null | head -n 1) && "
                    "[ -n \"$PPA_LIST_FILE\" ] && sed -i 's@ " + target_code + "@ " + target_code +
                    "@g' \"$PPA_LIST_FILE\" 2>/dev/null || true");
            }
        }
    }


    void DesktopManager::ubuntu_dde_or_dde_extras(std::string &dep_01) {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"UbuntuDDE or UbuntuDDE-extras\" --yes-button \"dde\" --no-button \"dde-extras\""
                                       " --yesno '前者为普通dde,后者除了dde本体外，还包含了dde额外软件包' 0 0");
        if (r.exit_code == 0) dep_01 = "ubuntudde-dde deepin-terminal";
        else dep_01 = "ubuntudde-dde ubuntudde-dde-extras";
    }


    void DesktopManager::fix_dde_dpkg_error_ext() {
        const char *postinst_files[] = {
            "/var/lib/dpkg/info/mincores-dkms.postinst",
            "/var/lib/dpkg/info/warm-sched.postinst"
        };
        for (const auto &pf: postinst_files) {
            if (fs::exists(pf)) {
                auto pf_content = SystemHelper::read_file(pf);
                size_t newline_pos = pf_content.find('\n');
                if (newline_pos != std::string::npos) {
                    pf_content.insert(newline_pos + 1, "return 0\n");
                    SystemHelper::write_file(pf, pf_content);
                }
            }
        }
        Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
    }


    void DesktopManager::modify_the_default_xfce_wallpaper() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string xml_file = home + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml";
        if (!fs::exists(xml_file)) {
            SystemHelper::write_file(fs::path(xml_file), generate_xfce_desktop_xml());
        }
    }


    void DesktopManager::modify_xfce_vnc0_wallpaper(const std::string &path) {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string xml_file = home + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml";
        if (fs::exists(xml_file) && fs::exists(path)) {
            Executor::shell("sed -i 's@<property name=\"image-path\" type=\"string\" value=\".*\"/>@"
                            "<property name=\"image-path\" type=\"string\" value=\"" + path + "\"/>@g' " + xml_file +
                            " 2>/dev/null || true");
        }
    }


    void DesktopManager::xfce_papirus_icon_theme_ext() {
        // 对应旧 Bash xfce_papirus_icon_theme (gui:1881-1888)
        download_papirus_icon_theme();
    }


    void DesktopManager::debian_xfce_wallpaper() {
        // 对应旧 Bash debian_xfce_wallpaper (gui:1974-1981)
        modify_the_default_xfce_wallpaper();
    }


    void DesktopManager::debian_download_mint_wallpaper() {
        // 对应旧 Bash debian_download_mint_wallpaper (gui:1969-1972)
        download_mint_backgrounds("ulyana");
    }


    void DesktopManager::debian_download_ubuntu_mate_wallpaper_ext() {
        // 对应旧 Bash debian_download_ubuntu_mate_wallpaper (gui:2155-2158)
        download_ubuntu_mate_wallpaper();
    }


    void DesktopManager::debian_download_xubuntu_xenial_wallpaper_ext() {
        // 对应旧 Bash debian_download_xubuntu_xenial_wallpaper (gui:2160-2165)
        download_xubuntu_wallpaper("xenial", "xubuntu-community-artwork/xenial");
    }


    void DesktopManager::set_linuxmint_wallpaper_vars(const std::string &mint_code,
                                                      std::string &out_name, std::string &out_path) {
        if (mint_code == "serena") {
            out_name = "mint_backgrounds_serena";
            out_path = "backgrounds/linuxmint-serena";
        } else if (mint_code == "sonya") {
            out_name = "mint_backgrounds_sonya";
            out_path = "backgrounds/linuxmint-sonya";
        } else if (mint_code == "sylvia") {
            out_name = "mint_backgrounds_sylvia";
            out_path = "backgrounds/linuxmint-sylvia";
        } else if (mint_code == "tara") {
            out_name = "mint_backgrounds_tara";
            out_path = "backgrounds/linuxmint-tara";
        } else if (mint_code == "tessa") {
            out_name = "mint_backgrounds_tessa";
            out_path = "backgrounds/linuxmint-tessa";
        } else if (mint_code == "tina") {
            out_name = "mint_backgrounds_tina";
            out_path = "backgrounds/linuxmint-tina";
        } else if (mint_code == "tricia") {
            out_name = "mint_backgrounds_tricia";
            out_path = "backgrounds/linuxmint-tricia";
        } else if (mint_code == "ulyana") {
            out_name = "mint_backgrounds_ulyana";
            out_path = "backgrounds/linuxmint-ulyana";
        } else if (mint_code == "ulyssa") {
            out_name = "mint_backgrounds_ulyssa";
            out_path = "backgrounds/linuxmint-ulyssa";
        } else if (mint_code == "sarah") {
            out_name = "mint_backgrounds_sarah";
            out_path = "backgrounds/linuxmint-sarah";
        } else {
            out_name = "mint_backgrounds_" + mint_code;
            out_path = "backgrounds/linuxmint-" + mint_code;
        }
    }


    std::string DesktopManager::pick_random_wallpaper_pack() {
        // 对应旧 Bash random_wallpaper_pack_01-05 (gui:2040-2119)
        int r = (std::time(nullptr) % 25) + 1;
        if (r <= 5) {
            return "xfce-stripes"; // xfce default
        } else if (r <= 10) {
            return "ubuntu-wallpapers/focal"; // ubuntu focal
        } else if (r <= 15) {
            return "mint-backgrounds/linuxmint-ulyana"; // mint
        } else if (r <= 20) {
            return "deepin-wallpapers"; // deepin
        } else {
            return "manjaro-2018"; // manjaro
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 5: 完整壁纸下载系统
    // ═══════════════════════════════════════════════════════════════


    void DesktopManager::download_xubuntu_wallpaper(const std::string &code_name, const std::string & /*folder_name*/) {
        Logger::step(_f("gui.wallpaper.xubuntu", code_name));
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        CommandBuilder("mkdir").add_flag("-pv").add_arg(home + "/Pictures/xubuntu-community-artwork").add_raw("2>/dev/null").execute();
        // 从 Ubuntu 镜像下载
        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/x/xubuntu-community-artwork/";
        Executor::shell("cd " + home + "/Pictures/xubuntu-community-artwork && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'xubuntu-community-wallpapers-" + code_name
                        + "' | "
                        "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o xubuntu-wp.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv xubuntu-wp.deb 2>/dev/null; tar -Jxvf data.tar.xz -C . 2>/dev/null) || true");
    }


    void DesktopManager::download_mint_backgrounds(const std::string &mint_code) {
        Logger::step(_f("gui.wallpaper.mint", mint_code));
        std::string repo = "https://mirrors.bfsu.edu.cn/linuxmint/pool/main/m/mint-backgrounds-" + mint_code + "/";
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        CommandBuilder("mkdir").add_flag("-pv").add_arg(home + "/Pictures/mint-backgrounds").add_raw("2>/dev/null").execute();
        Executor::shell("cd " + home + "/Pictures/mint-backgrounds && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'mint-backgrounds' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o mint-bg.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv mint-bg.deb 2>/dev/null; tar -Jxvf data.tar.xz -C . 2>/dev/null) || true");
    }


    void DesktopManager::download_ubuntu_mate_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_mate"));
        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-mate-artwork/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'ubuntu-mate-wallpapers-photos' | "
                        "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o umate.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv umate.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
    }


    void DesktopManager::install_fvwm_ext() {
        Logger::step(_("gui.fvwm.install"));
        auto family = resolved_family();
        if (family == DistroFamily::Debian) {
            auto os_rel = SystemHelper::read_file("/etc/os-release");
            bool is_debian_stable = (os_rel.find("buster") != std::string::npos ||
                                     os_rel.find("bullseye") != std::string::npos ||
                                     os_rel.find("bookworm") != std::string::npos);
            if (is_debian_stable) {
                Executor::passthrough(cfg_.install_command + " fvwm fvwm-icons fvwm-crystal 2>/dev/null || true");
            } else {
                Executor::passthrough(cfg_.install_command + " fvwm fvwm-icons 2>/dev/null || true");
                // 尝试从 Debian 镜像下载 fvwm-crystal
                Executor::shell("REPO_URL='https://mirrors.bfsu.edu.cn/debian/pool/main/f/fvwm-crystal/' && "
                    "cd /tmp && "
                    "LATEST=$(curl -L \"$REPO_URL\" 2>/dev/null | grep '\\.deb' | grep 'all' | "
                    "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                    "[ -n \"$LATEST\" ] && "
                    "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                    "-o fvwm-crystal.deb \"$REPO_URL$LATEST\" 2>/dev/null && "
                    "dpkg -i fvwm-crystal.deb 2>/dev/null || true");
            }
        } else {
            Executor::passthrough(cfg_.install_command + " fvwm fvwm-icons 2>/dev/null || true");
        }
    }


    void DesktopManager::tmoe_display_manager_systemctl(const std::string &dm_pkg, const std::string &dm_service) {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("gui.dm.menu_title") + "\" --menu \"" + _("gui.dm.menu_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("gui.dm.opt_install") + "\" "
                               "\"2\" \"" + _("gui.dm.opt_start") + "\" "
                               "\"3\" \"" + _("gui.dm.opt_stop") + "\" "
                               "\"4\" \"" + _("gui.dm.opt_enable") + "\" "
                               "\"5\" \"" + _("gui.dm.opt_disable") + "\" "
                               "\"0\" \"" + _("gui.dm.opt_back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") {
                Executor::passthrough(cfg_.install_command + " " + dm_pkg + " 2>/dev/null || true");
            } else if (ch == "2") {
                Executor::passthrough("systemctl start " + dm_service + " 2>/dev/null || "
                                      "service " + dm_service + " restart 2>/dev/null || true");
            } else if (ch == "3") {
                Executor::passthrough("systemctl stop " + dm_service + " 2>/dev/null || "
                                      "service " + dm_service + " stop 2>/dev/null || true");
            } else if (ch == "4") {
                Executor::passthrough("systemctl enable " + dm_service + " 2>/dev/null || "
                                      "rc-update add " + dm_service + " 2>/dev/null || true");
            } else if (ch == "5") {
                Executor::passthrough("systemctl disable " + dm_service + " 2>/dev/null || "
                                      "rc-update del " + dm_service + " 2>/dev/null || true");
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 最终补全: install_gui 交互流程 + 辅助函数
    // ═══════════════════════════════════════════════════════════════


    void DesktopManager::choose_plasma_wayland_or_x11() {
        // 对应旧 Bash choose_plasma_wayland_or_x11 (gui:2520-2526)
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"x11 or wayland\" --yes-button \"x11\" --no-button \"wayland\""
                                       " --yesno '默认推荐x11, wayland尚在实验阶段\\nWhich display protocol do you prefer?' 0 0");
        if (r.exit_code == 1) {
            plasma_wayland_env();
        }
    }


    void DesktopManager::download_papirus_icon_theme() {
        Logger::step(_("gui.papirus.install"));
        std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/p/papirus-icon-theme/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'papirus-icon-theme' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o papirus.deb '" + repo + "'\"$LATEST\" && "
                        "ar xv papirus.deb && tar -Jxvf data.tar.xz -C / 2>/dev/null && "
                        "update-icon-caches /usr/share/icons/Papirus /usr/share/icons/Papirus-Dark "
                        "/usr/share/icons/Papirus-Light /usr/share/icons/ePapirus 2>/dev/null &");
        set_default_xfce_icon_theme("Papirus");
    }


    std::string DesktopManager::generate_xfce_desktop_xml() {
        return gui_config::XFCE_DESKTOP_XML;
    }

    std::string DesktopManager::generate_xfce_terminal_rc() {
        return gui_config::XFCE_TERMINAL_RC;
    }

    std::string DesktopManager::generate_budgie_desktop_builtin() {
        return gui_config::BUDGIE_DESKTOP_BUILTIN;
    }

    std::string DesktopManager::generate_gnome_flashback_metacity() {
        return gui_config::GNOME_FLASHBACK_METACITY;
    }

    std::string DesktopManager::generate_gnome_session_classic() {
        return gui_config::GNOME_SESSION_CLASSIC;
    }

    std::string DesktopManager::generate_gnome_session_ubuntu() {
        return gui_config::GNOME_SESSION_UBUNTU;
    }

    std::string DesktopManager::generate_gnome_shell_x11() {
        return gui_config::GNOME_SHELL_X11;
    }

    std::string DesktopManager::generate_update_icon_caches_script() {
        return gui_config::UPDATE_ICON_CACHES_SCRIPT;
    }

    void DesktopManager::print_gnome_ascii() const {
        Logger::info("        .--.");
        Logger::info("       |o_o |     GNOME Foot");
        Logger::info("       |:_/ |");
        Logger::info("      //   \\\\ \\\\");
        Logger::info("     (|     | )");
        Logger::info("    /'\\\\_   _/`\\\\");
        Logger::info("    \\\\___)=(___/");
        Logger::info("  GNOME Desktop Environment");
        Logger::info("  https://www.gnome.org/");
    }
} // namespace tmoe::domain
