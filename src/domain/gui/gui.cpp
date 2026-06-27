#include "gui.hpp"

namespace fs = std::filesystem;

namespace tmoe::domain {
    namespace {
        // 提取当前发行版家族的通用辅助函数
        DistroFamily get_family(const TmoeConfig &cfg) {
            auto family = infer_family_from_config(cfg.linux_distro);
            if (family == DistroFamily::Unknown)
                family = PackageManager::detect_distro_family();
            return family;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 构造 / 析构
    // ═══════════════════════════════════════════════════════════════

    GUIManager::GUIManager(const TmoeConfig &cfg)
        : cfg_(cfg),
          vnc_manager_(cfg),
          desktop_manager_(cfg, vnc_manager_),
          remote_desktop_manager_(cfg, vnc_manager_, desktop_manager_) {
    }

    void GUIManager::first_configure_vnc(std::string_view desktop) {
        Logger::step(std::string(_("gui.vnc.config_title")) + " — " + std::string(desktop));
        auto family = get_family(cfg_);

        // 0.5. 卸载 udisks2 (对应旧 Bash remove_udisk_and_gvfs)
        if (cfg_.is_termux && family == DistroFamily::Debian) {
            auto os_rel_content = SystemHelper::read_file("/etc/os-release");
            bool is_old_distro = os_rel_content.find("Focal Fossa") != std::string::npos ||
                                 os_rel_content.find("focal") != std::string::npos ||
                                 os_rel_content.find("bionic") != std::string::npos ||
                                 os_rel_content.find("Bionic Beaver") != std::string::npos ||
                                 os_rel_content.find("Eoan Ermine") != std::string::npos ||
                                 os_rel_content.find("buster") != std::string::npos ||
                                 os_rel_content.find("stretch") != std::string::npos ||
                                 os_rel_content.find("jessie") != std::string::npos ||
                                 os_rel_content.find("Deepin 20") != std::string::npos ||
                                 os_rel_content.find("Uos 20") != std::string::npos;
            if (is_old_distro) {
                Logger::info(_("gui.vnc.proot_remove_udisks"));
                Executor::passthrough("apt purge -y --allow-change-held-packages ^udisks2 ^gvfs 2>/dev/null || true");
            }
        }

        // dpkg 修复
        if (Executor::has("apt-get")) {
            Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
        }

        vnc_manager_.configure_startvnc();
        vnc_manager_.configure_startxsdl();
        Executor::shell(CommandBuilder("chmod")
                        .add_arg("a+rx")
                        .add_arg("-v")
                        .add_arg("/usr/local/bin/startvnc")
                        .add_arg("/usr/local/bin/stopvnc")
                        .add_arg("/usr/local/bin/startxsdl")
                        .build_string() + " 2>/dev/null || true");

        // 5a. 安装 VNC 服务端字体依赖
        if (family == DistroFamily::Debian) {
            PackageManager::install("fonts-noto-color-emoji", family);
        }

        // 5b. 选择 VNC 服务端
        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        bool recommend_tiger = (d == "kde" || d == "gnome" || d == "cinnamon" ||
                                d == "dde" || d == "ukui" || d == "budgie" ||
                                d.find("startplasma") != std::string::npos);

        std::string prompt = recommend_tiger
                                 ? _("gui.vnc.tiger_recommend_prompt")
                                 : _("gui.vnc.server_select_prompt");

        auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--title").add_arg(_("gui.vnc.server_select_title"))
            .add_arg("--yes-button").add_arg(_("gui.vnc.tiger"))
            .add_arg("--no-button").add_arg(_("gui.vnc.tight"))
            .add_arg("--yesno").add_arg(prompt)
            .add_arg("0").add_arg("50")
            .build_string());
        if (r.exit_code == 0) {
            vnc_manager_.config().server = "tiger";
            vnc_manager_.config().server_bin = "tigervnc";
        } else if (r.exit_code == 1) {
            vnc_manager_.config().server = "tight";
            vnc_manager_.config().server_bin = "tightvnc";
        }

        vnc_manager_.modify_to_xfwm4_breeze_theme();

        // 5c. 设置 VNC 密码
        bool passwd_exists = fs::exists(vnc_manager_.config().passwd_file) &&
                             fs::file_size(vnc_manager_.config().passwd_file) > 0;
        if (passwd_exists) {
            // 已有密码 → 询问是否修改
            auto keep = Executor::passthrough(cfg_.tui_bin +
                " --title \"VNC PASSWORD\""
                " --yes-button \"Keep\""
                " --no-button \"Change\""
                " --yesno \"VNC password already exists.\\n\\n"
                "Choose Keep to retain the existing password,\\n"
                "or Change to set a new one.\" 0 0");
            if (keep.exit_code != 0) {  // 用户选了 Change
                vnc_manager_.configure_vnc_password();
            }
        } else {
            vnc_manager_.configure_vnc_password();
        }

        // 5d. 选择 VNC 端口
        if (desktop_manager_.is_auto_install_mode()) {
            vnc_manager_.config().display = 2;
            vnc_manager_.config().update_port();
        } else {
            auto port_r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.vnc.port"))
                .add_arg("--yes-button").add_arg("5902")
                .add_arg("--no-button").add_arg("5903")
                .add_arg("--yesno")
                .add_arg(_("gui.vnc.port_select_prompt"))
                .add_arg("0").add_arg("50")
                .build_string());
            if (port_r.exit_code == 0) {
                vnc_manager_.config().display = 2;
                vnc_manager_.config().update_port();
            } else if (port_r.exit_code == 1) {
                vnc_manager_.config().display = 3;
                vnc_manager_.config().update_port();
            }
        }

        // 5e. HiDPI 检测
        if (desktop_manager_.is_auto_install_mode()) {
            vnc_manager_.config().resolution_w = 1440;
            vnc_manager_.config().resolution_h = 720;
        } else {
            detect_and_configure_hidpi(desktop);
        }

        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";

        // Neko ASCII art
        Logger::info("               .::::..");
        Logger::info("    ::::rrr7QQJi::i:iirijQBBBQB.");
        Logger::info("    BBQBBBQBP. ......:::..1BBBB");
        Logger::info("    .BuPBBBX  .........r.  vBQL  :Y.");
        Logger::info("     rd:iQQ  ..........7L   MB    rr");
        Logger::info("      7biLX .::.:....:.:q.  ri    .");
        Logger::info("       JX1: .r:.r....i.r::...:.  gi5");
        Logger::info("       ..vr .7: 7:. :ii:  v.:iv :BQg");
        Logger::info("       : r:  7r:i7i::ri:DBr..2S");
        Logger::info("    i.:r:. .i:XBBK...  :BP ::jr   .7.");
        Logger::info("    r  i....ir r7.         r.J:   u.");
        Logger::info("   :..X: .. .v:           .:.Ji");
        Logger::info("  i. ..i .. .u:.     .   77: si   1Q");
        Logger::info(" ::.. .r .. :P7.r7r..:iLQQJ: rv   ..");
        Logger::info("7  iK::r  . ii7r LJLrL1r7DPi iJ     r");
        Logger::info("  .  ::.:   .  ri 5DZDBg7JR7.:r:   i.");
        Logger::info(" .Pi r..r7:     i.:XBRJBY:uU.ii:.  .");
        Logger::info(" QB rJ.:rvDE: .. ri uv . iir.7j r7.");
        Logger::info("iBg ::.7251QZ. . :.      irr:Iu: r.");
        Logger::info(" QB  .:5.71Si..........  .sr7ivi:U");
        Logger::info(" 7BJ .7: i2. ........:..  sJ7Lvr7s");
        Logger::info("  jBBdD. :. ........:r... YB  Bi");
        Logger::info("     :7j1.                 :  :");

        // 完整分辨率检测
        std::string wm_size_path = "/usr/local/etc/tmoe-linux/wm_size.txt";
        std::string wm_size_home = home + "/.tmoe-linux/wm_size.txt";
        std::string res;
        std::string tmo_high_dpi = "default";
        if (fs::exists(wm_size_path) || fs::exists(wm_size_home)) {
            std::string wmf = fs::exists(wm_size_path) ? wm_size_path : wm_size_home;
            auto content = SystemHelper::read_file(fs::path(wmf));
            auto xpos = content.find('x');
            if (xpos != std::string::npos) {
                try {
                    int _w = std::stoi(content.substr(0, xpos));
                    int _h = std::stoi(content.substr(xpos + 1));
                    int _swapped_w = (_w < _h) ? _h : _w;
                    int _swapped_h = (_w < _h) ? _w : _h;
                    res = std::to_string(_swapped_w) + "x" + std::to_string(_swapped_h);
                    if (_swapped_w >= 2340) tmo_high_dpi = "true";
                } catch (...) {
                }
            }
        }

        // 分辨率同步到所有配置文件
        if (!res.empty() && vnc_manager_.config().resolution_w > 0 && vnc_manager_.config().resolution_h > 0) {
            std::string res_str = std::to_string(vnc_manager_.config().resolution_w) + "x" + std::to_string(
                                      vnc_manager_.config().resolution_h);
            // geometry 已通过 configure_vnc_defaults() + ~/.vnc/config 持久化，无需 sed 旧路径
            Executor::shell(CommandBuilder("sed")
                            .add_arg("-i")
                            .add_arg("-E")
                            .add_arg("s@^(VNC_RESOLUTION)=.*@\\1=" + res_str + "@")
                            .add_arg("/usr/local/bin/startvnc")
                            .build_string() + " 2>/dev/null || true");
            Executor::shell(CommandBuilder("sed")
                            .add_arg("-i")
                            .add_arg("-E")
                            .add_arg("s@^(TMOE_X11_RESOLUTION)=.*@\\1=" + res_str + "@")
                            .add_arg("/usr/local/bin/startx11vnc")
                            .build_string() + " 2>/dev/null || true");
        }

        // 5f. 权限修复
        if (home != "/root") {
            // 预建桌面环境需要的用户目录（避免 "No such file or directory" 错误）
            Executor::shell("mkdir -p " + home + "/.local/share "
                            + home + "/.config/autostart "
                            + home + "/.cache/sessions 2>/dev/null || true");
            Executor::passthrough(CommandBuilder("chown")
                                  .add_arg("-R")
                                  .add_arg("${SUDO_USER:-$(id -un)}:${SUDO_USER:-$(id -gn)}")
                                  .add_arg(home + "/.vnc")
                                  .add_arg(home + "/.Xauthority")
                                  .add_arg(home + "/.ICEauthority")
                                  .add_arg(home + "/.cache")
                                  .add_arg(home + "/.dbus")
                                  .add_arg(home + "/.local")
                                  .build_string() + " 2>/dev/null || true");
        }

        vnc_manager_.configure_vnc_defaults();

        // 6. 创建 ~/startvnc 符号链接
        if (!fs::exists(home + "/startvnc"))
            Executor::shell(CommandBuilder("ln")
                            .add_arg("-sf")
                            .add_arg("/usr/local/bin/startvnc")
                            .add_arg(home + "/startvnc")
                            .build_string() + " 2>/dev/null || true");

        // 复制 .vnc 到 /root
        if (home != "/root") {
            std::error_code ec;
            fs::copy(home + "/.vnc", "/root/.vnc", fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                     ec);
            Executor::shell(CommandBuilder("chown")
                            .add_arg("-R")
                            .add_arg("0:0")
                            .add_arg("/root/.vnc")
                            .build_string() + " 2>/dev/null || true");
        }

        // 7. WSL 环境网络处理
        if (cfg_.is_wsl) {
            Logger::info(_("gui.wsl_win10_xserver"));
            Logger::info(_("gui.wsl_firewall_hint"));
            Logger::info(_("gui.wsl_hidpi_hint"));

            auto wsl_ver = Executor::shell("uname -r | cut -d '-' -f 2 2>/dev/null");
            std::string ver = wsl_ver.ok() ? wsl_ver.stdout_data : "";
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r')) ver.pop_back();
            if (ver == "microsoft") {
                auto wsl_r = Executor::shell("ip route list table 0 2>/dev/null | head -1 | "
                    "awk -F 'default via ' '{print $2}' | awk '{print $1}'");
                std::string wsl_ip = wsl_r.ok() ? wsl_r.stdout_data : "";
                while (!wsl_ip.empty() && (wsl_ip.back() == '\n' || wsl_ip.back() == '\r')) wsl_ip.pop_back();
                if (!wsl_ip.empty()) {
                    Executor::shell(CommandBuilder("export")
                                    .add_arg("PULSE_SERVER=" + wsl_ip)
                                    .build_string() + " 2>/dev/null || true");
                    Executor::shell(CommandBuilder("export")
                                    .add_arg("DISPLAY=" + wsl_ip + ":0")
                                    .build_string() + " 2>/dev/null || true");
                    Logger::info(std::string(_("gui.wsl_ip_modified")) + wsl_ip);
                }
            } else {
                Logger::info(_("gui.wsl1_detected"));
                Executor::shell(CommandBuilder("export")
                                .add_arg("DISPLAY=127.0.0.1:0")
                                .build_string() + " 2>/dev/null || true");
            }
            Logger::info(_("gui.wsl_wslg_hint"));
        }

        Logger::info("------------------------");
        Logger::info(_("gui.audio_service_info"));
        Logger::info(_("gui.audio.termux_pulseaudio_hint"));
        Logger::info(_("gui.audio.android_pulseaudio_hint"));
        Logger::info(_("gui.audio.win10_pulseaudio_hint"));
        Logger::info("------------------------");
        Logger::info(_("gui.vnc_x_startup_info"));
        Logger::info(_("gui.vnc.startvnc_both_hint"));
        Logger::info(_("gui.vnc.startvnc_container_hint"));
        Logger::info(_("gui.vnc.startvnc_english_hint"));
        Logger::info(_("gui.vnc.startxsdl_english_hint"));
        Logger::info(_("gui.vnc.startxsdl_host_hint"));
        Logger::info("------------------------");
        Logger::info(_("gui.audio.android_host_hint"));
        Logger::info(_("gui.audio.win10_host_hint"));
        if (Executor::has("apt-get")) {
            Logger::info(_("gui.vnc.tightvnc_tigervnc_cmd_hint"));
        }
        Logger::info(_("gui.vnc.configured_x11vnc_next"));
        Logger::info("------------------------");

        Logger::info(std::string(_("gui.section_three")));
        vnc_manager_.check_xvnc_command();
        remote_desktop_manager_.x11vnc_warning();
        Logger::press_enter();
        auto x11vnc_confirm = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--yesno")
            .add_arg(std::string(_("gui.confirm_x11vnc")))
            .add_arg("0").add_arg("0")
            .build_string());
        if (x11vnc_confirm.exit_code == 0) {
            remote_desktop_manager_.configure_x11vnc_remote_desktop_session();
        }
        Logger::info("------------------------");

        Logger::info(std::string(_("gui.section_four")) + "：");
        Logger::info(_("gui.vnc.unlock_achievement_hint"));
        // 修复 sudo 提权安装导致的 root 归属问题
        vnc_manager_.fix_vnc_permissions();

        Logger::press_enter();
        remote_desktop_manager_.do_you_want_to_configure_novnc();
    }

    bool GUIManager::configure_xsdl() {
        Logger::step(_("gui.xsdl.configure_step"));

        std::string display_cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(std::string(_("gui.xsdl_config_title")))
                .add_arg("--menu").add_arg(std::string(_("gui.xsdl_config_prompt")))
                .add_arg("0").add_arg("0").add_arg("0")
                .add_arg("1").add_arg(std::string(_("gui.xsdl_display_local")))
                .add_arg("2").add_arg(std::string(_("gui.xsdl_display_custom")))
                .build_string();

        std::string choice = Executor::tui_select(display_cmd);
        std::string display = "127.0.0.1:0";

        if (choice == "2") {
            std::string input_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(std::string(_("gui.xsdl_custom_title")))
                    .add_arg("--inputbox").add_arg(std::string(_("gui.xsdl_custom_input")))
                    .add_arg("10").add_arg("40").add_arg("127.0.0.1:0")
                    .build_string();
            display = Executor::tui_select(input_cmd);
        }

        Logger::ok(std::string(_("gui.xsdl.display_set")) + display);
        return true;
    }

    bool GUIManager::start_xsdl() {
        Logger::step(_("gui.xsdl.start_step"));

        if (cfg_.is_wsl) {
            vnc_manager_.detect_wsl_environment();
        }

        std::string display = cfg_.is_wsl ? (vnc_manager_.config().windows_ip + ":0") : "127.0.0.1:0";
        std::string env = "export DISPLAY=" + display + " "
                          "export PULSE_SERVER=tcp:" + display.substr(0, display.find(':')) + ":" +
                          std::to_string(vnc_manager_.config().pulse_port);

        if (cfg_.is_wsl) {
            Logger::info(_("gui.xsdl.wsl_detect_vcxsrv"));
            Logger::info("DISPLAY=" + display);
        }

        std::string cmd = env + " xfce4-session 2>/dev/null || " + env + " startxfce4 2>/dev/null || "
                          + env + " openbox 2>/dev/null &";
        Executor::passthrough(cmd);

        Logger::ok(std::string(_("gui.xsdl.started")) + display);
        return true;
    }

    bool GUIManager::configure_wsl_pulseaudio() {
        Logger::step(_("gui.wsl.pulseaudio_config_step"));

        if (!cfg_.is_wsl) {
            Logger::warn(_("gui.wsl.not_in_wsl"));
            return false;
        }

        vnc_manager_.detect_wsl_environment();

        std::string pulse_server = "tcp:" + vnc_manager_.config().windows_ip + ":" +
                                   std::to_string(vnc_manager_.config().pulse_port);

        std::string bashrc = std::getenv("HOME")
                                 ? std::string(std::getenv("HOME")) + "/.bashrc"
                                 : "/root/.bashrc";
        SystemHelper::append_file(fs::path(bashrc),
                                  "\n# tmoe WSL PulseAudio (已自动配置)\n"
                                  "export PULSE_SERVER=" + pulse_server + "\n");

        Logger::ok(std::string(_("gui.wsl.pulseaudio_configured")) + pulse_server);
        Logger::info(_("gui.wsl.pulseaudio_start_hint"));
        return true;
    }

    bool GUIManager::start_wslg(int display_port) {
        Logger::step(_("gui.wslg.start_step"));

        if (!cfg_.is_wsl) {
            Logger::warn(_("gui.wslg.win11_only"));
            return false;
        }

        std::error_code ec;
        fs::remove("/tmp/.X" + std::to_string(display_port) + "-lock", ec);
        fs::remove("/tmp/.X11-unix/X" + std::to_string(display_port), ec);

        Executor::shell(CommandBuilder("stopvnc")
                        .add_arg("-no-stop-dbus")
                        .build_string() + " 2>/dev/null || true");
        vnc_manager_.launch_dbus_daemon();

        auto xwayland_cmd = CommandBuilder("Xwayland")
                .add_arg(":" + std::to_string(display_port))
                .add_arg("-noreset");
        Executor::passthrough("unset WAYLAND_DISPLAY; " + xwayland_cmd.build_string() + " &");
        Executor::shell("sleep 1");

        Logger::ok(std::string(_("gui.wslg.started")) + std::to_string(display_port));
        Logger::info(_("gui.wslg.gui_ready_hint"));
        return true;
    }

    bool GUIManager::beautify_desktop() {
        Logger::step(_("gui.beautify_step"));
        return true;
    }

    bool GUIManager::install_theme(std::string_view theme) {
        Logger::step(std::string(_("gui.beautify.install_theme")) + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool GUIManager::install_icon_theme(std::string_view theme) {
        Logger::step(std::string(_("gui.beautify.install_icon_theme")) + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::icon_theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool GUIManager::set_wallpaper(std::string_view path) {
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

    bool GUIManager::install_dock() {
        Logger::step(_("gui.beautify.install_plank"));
        return PackageManager::install("plank", get_family(cfg_));
    }

    // ═══════════════════════════════════════════════════════════════
    // PulseAudio
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::start_pulseaudio_bridge() const {
        if (cfg_.is_wsl) {
            Logger::info(_("gui.pulse.wsl_provided"));
            Logger::info(_("gui.pulse.wsl_manual_hint"));
            return true;
        }

        if (!Executor::has("pulseaudio")) {
            Logger::warn(_("gui.pulse.not_installed"));
            return false;
        }

        Logger::step(_("gui.pulse.bridge_start"));

        std::string prefix;
        if (!cfg_.is_termux && std::getenv("SUDO_USER")) {
            prefix = std::string("su - ") + std::getenv("SUDO_USER") + " -c ";
        }

        std::string cmd =
                prefix +
                "\"pulseaudio --start --load=\\\"module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1\\\" --exit-idle-time=-1\"";

        return Executor::passthrough(cmd + " >/dev/null 2>&1").ok();
    }

    // ═══════════════════════════════════════════════════════════════
    // TUI 交互式菜单
    // ═══════════════════════════════════════════════════════════════

    void GUIManager::run_gui_menu() {
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(std::string(_("gui.main_title")))
                    .add_arg("--menu").add_arg(std::string(_("gui.main_menu_prompt")))
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("1").add_arg(std::string(_("gui.install_de")))
                    .add_arg("2").add_arg(std::string(_("gui.remote_title")))
                    .add_arg("3").add_arg(std::string(_("gui.beautify")))
                    .add_arg("0").add_arg(std::string(_("menu.tui.back_upper")))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_desktop_install_menu();
            } else if (choice == "2") {
                run_remote_desktop_menu();
            } else if (choice == "3") {
                run_beautification_menu();
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_vnc_config_menu() {
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.vnc_config_title"))
                    .add_arg("--menu")
                    .add_arg(_("gui.vnc_config_prompt"))
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("edit_startvnc").add_arg(_("gui.vnc.edit_startvnc"))
                    .add_arg("1").add_arg(std::string(_("gui.vnc_password")))
                    .add_arg("2").add_arg(std::string(_("gui.vnc_switch_server")))
                    .add_arg("3").add_arg(std::string(_("gui.vnc_resolution")) + " " +
                                          std::to_string(vnc_manager_.config().resolution_w) + "x" +
                                          std::to_string(vnc_manager_.config().resolution_h) + ")")
                    .add_arg("4").add_arg(std::string(_("gui.vnc_port")) + " " +
                                          std::to_string(vnc_manager_.config().rfb_port) + ")")
                    .add_arg("5").add_arg(std::string(_("gui.vnc_depth")) + " " +
                                          std::to_string(vnc_manager_.config().pixel_depth) + ")")
                    .add_arg("6").add_arg(std::string(_("gui.vnc_zlib")) + " " +
                                          std::to_string(vnc_manager_.config().zlib_level) + ")")
                    .add_arg("7").add_arg(std::string(_("gui.vnc_pulseaudio")))
                    .add_arg("8").add_arg(_("gui.vnc.edit_xsession"))
                    .add_arg("9").add_arg(_("gui.vnc.edit_tigervnc_config"))
                    .add_arg("10").add_arg(_("gui.vnc.window_scaling"))
                    .add_arg("11").add_arg(_("gui.vnc.wsl_pulseaudio"))
                    .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "edit_startvnc") {
                Executor::passthrough(
                    "${EDITOR:-nano} /usr/local/bin/startvnc 2>/dev/null || nano /usr/local/bin/startvnc");
            } else if (choice == "1") {
                vnc_manager_.configure_vnc_password();
            } else if (choice == "2") {
                vnc_manager_.choose_vnc_server();
                vnc_manager_.configure_vnc_defaults();
            } else if (choice == "3") {
                std::string res_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.resolution_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.resolution_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("1440x720").add_arg(std::string(_("gui.resolution_1440x720")))
                        .add_arg("1280x720").add_arg(std::string(_("gui.resolution_1280x720")))
                        .add_arg("1920x1080").add_arg(std::string(_("gui.resolution_1920x1080")))
                        .add_arg("2560x1440").add_arg(std::string(_("gui.resolution_2560x1440")))
                        .add_arg("1024x768").add_arg(std::string(_("gui.resolution_1024x768")))
                        .add_arg("custom").add_arg(std::string(_("gui.resolution_custom")))
                        .build_string();
                std::string res = Executor::tui_select(res_cmd);
                if (!res.empty() && res != "custom") {
                    auto xpos = res.find('x');
                    if (xpos != std::string::npos) {
                        vnc_manager_.config().resolution_w = std::stoi(res.substr(0, xpos));
                        vnc_manager_.config().resolution_h = std::stoi(res.substr(xpos + 1));
                        vnc_manager_.configure_vnc_defaults();
                    }
                }
            } else if (choice == "4") {
                std::string port_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.port_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.port_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("5902").add_arg(std::string(_("gui.port_5902")))
                        .add_arg("5903").add_arg(std::string(_("gui.port_5903")))
                        .add_arg("5901").add_arg(std::string(_("gui.port_5901")))
                        .build_string();
                std::string port = Executor::tui_select(port_cmd);
                if (!port.empty()) {
                    int p = std::stoi(port);
                    vnc_manager_.config().display = p - 5900;
                    vnc_manager_.config().rfb_port = p;
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@tmoe-linux.*:.*@tmoe-linux :" +
                                             std::to_string(vnc_manager_.config().display) + "@")
                                    .add_arg("/usr/local/bin/startvnc")
                                    .build_string() + " 2>/dev/null || true");
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@VNC_DISPLAY=.*@VNC_DISPLAY=" +
                                             std::to_string(vnc_manager_.config().display) + "@")
                                    .add_arg("/usr/local/bin/startvnc")
                                    .build_string() + " 2>/dev/null || true");
                }
            } else if (choice == "5") {
                std::string depth_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.depth_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.depth_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("24").add_arg(std::string(_("gui.depth_24")))
                        .add_arg("16").add_arg(std::string(_("gui.depth_16")))
                        .build_string();
                std::string depth = Executor::tui_select(depth_cmd);
                if (!depth.empty()) vnc_manager_.config().pixel_depth = std::stoi(depth);
            } else if (choice == "6") {
                std::string zlib_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.zlib_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.zlib_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("0").add_arg(std::string(_("gui.zlib_0")))
                        .add_arg("3").add_arg(std::string(_("gui.zlib_3")))
                        .add_arg("6").add_arg(std::string(_("gui.zlib_6")))
                        .add_arg("9").add_arg(std::string(_("gui.zlib_9")))
                        .build_string();
                std::string zlib = Executor::tui_select(zlib_cmd);
                if (!zlib.empty()) vnc_manager_.config().zlib_level = std::stoi(zlib);
            } else if (choice == "7") {
                std::string pa_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.vnc.pulse_server_title"))
                        .add_arg("--inputbox")
                        .add_arg(_("gui.vnc.pulse_server_prompt"))
                        .add_arg("20").add_arg("50")
                        .build_string();
                std::string pa_addr = Executor::tui_select(pa_cmd);
                if (!pa_addr.empty()) {
                    std::string pa_script = "/usr/local/bin/startvnc";
                    auto pa_content = SystemHelper::read_file(fs::path(pa_script));
                    if (pa_content.find("PULSE_SERVER") != std::string::npos) {
                        Executor::shell(CommandBuilder("sed")
                                        .add_arg("-i")
                                        .add_arg("s@^export PULSE_SERVER=.*@export PULSE_SERVER=" + pa_addr + "@")
                                        .add_arg(pa_script)
                                        .build_string() + " 2>/dev/null || true");
                    } else {
                        Executor::shell(CommandBuilder("sed")
                                        .add_arg("-i")
                                        .add_arg("4 a\\export PULSE_SERVER=" + pa_addr)
                                        .add_arg(pa_script)
                                        .build_string() + " 2>/dev/null || true");
                    }
                    Logger::ok(std::string(_("gui.vnc.pulse_server_updated")) + pa_addr);
                }
            } else if (choice == "8") {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/X11/xinit/Xsession");
            } else if (choice == "9") {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                std::string cfg_path = vnc_manager_.config().vnc_home_dir.string() + "/config";
                Executor::passthrough(editor + " " + cfg_path);
            } else if (choice == "10") {
                std::string scale_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.vnc.scaling_title"))
                        .add_arg("--inputbox")
                        .add_arg(_("gui.vnc.scaling_prompt"))
                        .add_arg("12").add_arg("50").add_arg("1")
                        .build_string();
                std::string scale = Executor::tui_select(scale_cmd);
                if (!scale.empty() && (scale == "1" || scale == "2")) {
                    Executor::shell(CommandBuilder("dbus-launch")
                                    .add_arg("xfconf-query")
                                    .add_arg("-c").add_arg("xsettings")
                                    .add_arg("-t").add_arg("int")
                                    .add_arg("-np").add_arg("/Gdk/WindowScalingFactor")
                                    .add_arg("-s").add_arg(scale)
                                    .build_string() + " 2>/dev/null || true");
                    if (scale == "2") {
                        auto os_rel_content = SystemHelper::read_file("/etc/os-release");
                        if (os_rel_content.find("Focal Fossa") != std::string::npos ||
                            os_rel_content.find("focal") != std::string::npos) {
                            Executor::shell(CommandBuilder("dbus-launch")
                                            .add_arg("xfconf-query")
                                            .add_arg("-c").add_arg("xfwm4")
                                            .add_arg("-t").add_arg("string")
                                            .add_arg("-np").add_arg("/general/theme")
                                            .add_arg("-s").add_arg("Kali-Light-xHiDPI")
                                            .build_string() + " 2>/dev/null || true");
                            Logger::info(_("gui.vnc.focal_hidpi_theme"));
                        }
                    }
                    Logger::ok(std::string(_("gui.vnc.scaling_set")) + scale);
                }
            } else if (choice == "11") {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /usr/local/etc/tmoe-linux/wsl_pulse_audio");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_desktop_install_menu() {
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.de_install_title"))
                    .add_arg("--menu").add_arg("")
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("1").add_arg(_("gui.de_rootless"))
                    .add_arg("2").add_arg(_("gui.de_rootful"))
                    .add_arg("3").add_arg(_("gui.de_install_wm"))
                    .add_arg("4").add_arg(_("gui.de_install_dm"))
                    .add_arg("0").add_arg(_("menu.tui.back"))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_rootless_de_menu();
            } else if (choice == "2") {
                run_rootful_de_menu();
            } else if (choice == "3") {
                run_wm_menu();
            } else if (choice == "4") {
                run_dm_menu();
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_rootless_de_menu() {
        while (true) {
            CommandBuilder menu_cmd(cfg_.tui_bin);
            menu_cmd.add_arg("--title").add_arg(_("gui.de_rootless"))
                    .add_arg("--menu").add_arg("")
                    .add_arg("0").add_arg("0").add_arg("0");
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root && !d.is_window_manager) {
                    menu_cmd.add_arg(std::to_string(idx++)).add_arg(d.name + " (" + d.compat_notes + ")");
                }
            }
            menu_cmd.add_arg("0").add_arg(_("menu.tui.back"));
            auto ch = Executor::tui_select(menu_cmd.build_string());
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root && !d.is_window_manager) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_desktop(d.id);
                        first_configure_vnc(d.id);
                        desktop_manager_.after_desktop_install_hint();
                        break;
                    }
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_rootful_de_menu() {
        while (true) {
            CommandBuilder menu_cmd(cfg_.tui_bin);
            menu_cmd.add_arg("--title").add_arg(_("gui.de_rootful"))
                    .add_arg("--menu").add_arg("")
                    .add_arg("0").add_arg("0").add_arg("0");
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.requires_root && !d.is_window_manager) {
                    menu_cmd.add_arg(std::to_string(idx++)).add_arg(d.name + " (" + d.compat_notes + ")");
                }
            }
            menu_cmd.add_arg("0").add_arg(_("menu.tui.back"));
            auto ch = Executor::tui_select(menu_cmd.build_string());
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.requires_root && !d.is_window_manager) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_desktop(d.id);
                        first_configure_vnc(d.id);
                        desktop_manager_.after_desktop_install_hint();
                        break;
                    }
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_wm_menu() {
        while (true) {
            CommandBuilder wm_menu(cfg_.tui_bin);
            wm_menu.add_arg("--title").add_arg(std::string(_("gui.wm_title")))
                    .add_arg("--menu").add_arg(std::string(_("gui.wm_prompt")))
                    .add_arg("0").add_arg("0").add_arg("0");
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.is_window_manager) {
                    wm_menu.add_arg(std::to_string(idx++)).add_arg(
                        d.name + (d.compat_notes.empty() ? "" : " (" + d.compat_notes + ")"));
                }
            }
            wm_menu.add_arg("0").add_arg(std::string(_("menu.tui.back")));
            auto ch = Executor::tui_select(wm_menu.build_string());
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.is_window_manager) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_window_manager(d.id);
                        first_configure_vnc(d.id);
                        desktop_manager_.after_desktop_install_hint();
                        break;
                    }
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_dm_menu() {
        std::string dm_menu = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(std::string(_("gui.dm_title")))
                .add_arg("--menu").add_arg(std::string(_("gui.dm_prompt")))
                .add_arg("0").add_arg("0").add_arg("0")
                .add_arg("1").add_arg(_("gui.dm.lightdm"))
                .add_arg("2").add_arg(_("gui.dm.sddm"))
                .add_arg("3").add_arg(_("gui.dm.gdm"))
                .add_arg("4").add_arg(_("gui.dm.slim"))
                .add_arg("5").add_arg(_("gui.dm.lxdm"))
                .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                .build_string();
        auto ch = Executor::tui_select(dm_menu);
        auto fam = get_family(cfg_);
        if (ch == "1") {
            PackageManager::install({"lightdm", "lightdm-gtk-greeter"}, fam);
            desktop_manager_.tmoe_display_manager_systemctl("lightdm", "lightdm");
        } else if (ch == "2") {
            PackageManager::install({"sddm", "sddm-theme-breeze"}, fam);
            desktop_manager_.tmoe_display_manager_systemctl("sddm", "sddm");
        } else if (ch == "3") {
            PackageManager::install("gdm3", fam);
            desktop_manager_.tmoe_display_manager_systemctl("gdm3", "gdm");
        } else if (ch == "4") {
            PackageManager::install("slim", fam);
            desktop_manager_.tmoe_display_manager_systemctl("slim", "slim");
        } else if (ch == "5") {
            PackageManager::install("lxdm", fam);
            desktop_manager_.tmoe_display_manager_systemctl("lxdm", "lxdm");
        }
    }

    void GUIManager::run_xsdl_config_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.xsdl.config_menu_title"))
                    .add_arg("--menu")
                    .add_arg(_("gui.xsdl.config_menu_prompt"))
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg(_("gui.xsdl.pulse_port"))
                    .add_arg("2").add_arg(_("gui.xsdl.display_number"))
                    .add_arg("3").add_arg(_("gui.xsdl.ip_address"))
                    .add_arg("4").add_arg(_("gui.xsdl.edit_manual"))
                    .add_arg("5").add_arg(_("gui.xsdl.display_switch"))
                    .add_arg("6").add_arg(_("gui.xsdl.vcxsrv_port"))
                    .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            std::string script = "/usr/local/bin/startxsdl";
            if (ch == "1") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.pulse_port_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.pulse_port_prompt"))
                        .add_arg("10").add_arg("50")
                        .build_string();
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@PULSE_SERVER=.*@PULSE_SERVER=\"tcp:localhost:" + val + "\"@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
            } else if (ch == "2") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.display_num_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.display_num_prompt"))
                        .add_arg("10").add_arg("50").add_arg("0")
                        .build_string();
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@DISPLAY_NUM=.*@DISPLAY_NUM=" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
            } else if (ch == "3") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.ip_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.ip_prompt"))
                        .add_arg("10").add_arg("50").add_arg("127.0.0.1")
                        .build_string();
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@XSDL_IP=.*@XSDL_IP=" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
            } else if (ch == "4") {
                Executor::passthrough("${EDITOR:-nano} " + script + " 2>/dev/null || nano " + script);
            } else if (ch == "5") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--yesno").add_arg(_("gui.xsdl.display_switch_prompt"))
                        .add_arg("0").add_arg("0")
                        .build_string();
                auto r = Executor::passthrough(cmd);
                if (r.exit_code == 0)
                    Executor::shell(
                        "if grep -q '^export.*DISPLAY' " + script + "; then " +
                        CommandBuilder("sed").add_arg("-i").add_arg("/export DISPLAY=/d").add_arg(script)
                        .build_string() +
                        "; else echo 'export DISPLAY=:0' >> " + script + "; fi 2>/dev/null || true");
            } else if (ch == "6") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.vcxsrv_port_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.vcxsrv_port_prompt"))
                        .add_arg("10").add_arg("50").add_arg(":0")
                        .build_string();
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@VCXSRV_DISPLAY=.*@VCXSRV_DISPLAY=" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_beautification_menu() {
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

    void GUIManager::docker_auto_install_gui_env(std::string_view /*desktop*/) {
        Logger::step(_("gui.docker_env_prep"));

        ensure_tmoe_symlink();

        auto family = get_family(cfg_);
        bool is_debian = (family == DistroFamily::Debian);
        bool is_ubuntu = (is_debian && cfg_.sub_distro == "ubuntu");
        bool is_kali = (is_debian && cfg_.sub_distro == "kali");

        if (is_debian && !Executor::has("add-apt-repository") && Executor::has("apt-get")) {
            PackageManager::install("software-properties-common", family);
        }

        if (!Executor::has("aria2c")) {
            PackageManager::install("aria2", family);
        }

        if (is_ubuntu) {
            PackageManager::install("^language-pack-zh", family);
        }

        if (is_kali) {
            PackageManager::install("debian-archive-keyring", family);
        }

        desktop_manager_.download_iosevka_ttf_font_ext();
        desktop_manager_.preconfigure_gui_dependencies();

        DistroFamily fam = family;
        bool af = false, ae = false, av = false, ac = true, ak = false;
        std::string kt = "kali-linux-arm";
        if (fam == DistroFamily::Alpine) {
            af = false;
            ae = false;
        } else if (fam == DistroFamily::RedHat) {
            af = false;
            ae = true;
            av = true;
        } else if (fam == DistroFamily::Debian || fam == DistroFamily::Arch) {
            af = false;
            ae = true;
            PackageManager::install("baobab", fam);
            PackageManager::install("bleachbit", fam);
            av = true;
        }

        if (is_kali) {
            kt = "kali-linux-arm";
            ak = true;
        }

        ac = true;
        desktop_manager_.set_auto_install_flags(af, ae, av, ac, ak, kt);

        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        fs::create_directories(home + "/.vnc");
        SystemHelper::write_file(home + "/.vnc/passwd", "please delete the invalid passwd file\n");

        Logger::ok(_("gui.docker_env_done"));
    }

    bool GUIManager::auto_install_gui(std::string_view desktop) {
        Logger::info(_f("gui.auto_install.mode", std::string(desktop)));

        set_auto_install_mode(true);
        docker_auto_install_gui_env(desktop);

        if (!desktop_manager_.install_desktop(desktop)) {
            Logger::error(_("gui.auto_install.failed"));
            return false;
        }

        desktop_manager_.install_fcitx();
        vnc_manager_.fix_vnc_permissions();
        vnc_manager_.deploy_startup_scripts();
        vnc_manager_.fix_vnc_dbus();

        bool ok = vnc_manager_.start_vnc();

        if (ok) {
            Logger::ok(_("gui.auto_install.complete"));
            Logger::info(_f("gui.auto_install.vnc_address", vnc_manager_.get_vnc_connection_uri()));
            Logger::info(_("gui.auto_install.password"));
        }

        return ok;
    }

    bool GUIManager::install_iosevka_font() {
        Logger::step(_("gui.font.install_iosevka"));

        if (fs::exists("/usr/share/fonts/truetype/iosevka") ||
            fs::exists("/usr/share/fonts/iosevka") ||
            fs::exists("/usr/local/share/fonts/iosevka")) {
            Logger::ok(_("gui.font.iosevka_installed"));
            return true;
        }

        auto family = get_family(cfg_);
        if (PackageManager::install("fonts-iosevka", family)) {
            Logger::ok(_("gui.font.iosevka_done"));
            return true;
        }

        const char *iosevka_url = "https://github.com/be5invis/Iosevka/releases/download/v28.1.0/"
                "PkgTTF-Iosevka-28.1.0.zip";

        std::string font_dir = "/usr/local/share/fonts/iosevka";
        fs::create_directories(font_dir);

        Logger::info(_("gui.font.downloading_iosevka"));
        std::string dl_cmd = "cd /tmp && (" +
                             CommandBuilder("wget").add_arg("-q").add_arg("--timeout=30")
                             .add_arg(std::string(iosevka_url)).add_arg("-O").add_arg("iosevka.zip")
                             .build_string() + " 2>/dev/null || " +
                             CommandBuilder("curl").add_arg("-sL")
                             .add_arg(std::string(iosevka_url)).add_arg("-o").add_arg("iosevka.zip")
                             .build_string() + " 2>/dev/null)" +
                             " && " + CommandBuilder("unzip").add_arg("-o").add_arg("iosevka.zip")
                             .add_arg("-d").add_arg(font_dir).build_string() +
                             " 2>/dev/null && rm -f iosevka.zip";

        if (Executor::passthrough(dl_cmd).ok()) {
            Executor::passthrough("fc-cache -fv 2>/dev/null || true");
            Logger::ok(std::string(_("gui.font.iosevka_done_path")) + font_dir);
            return true;
        }

        Logger::warn(_("gui.font.iosevka_dl_failed"));
        return false;
    }

    // ═══════════════════════════════════════════════════════════════
    // HiDPI 检测与配置
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::detect_and_configure_hidpi(std::string_view desktop) {
        std::string wm_size_file = "/usr/local/etc/tmoe-linux/wm_size.txt";
        int orig_w = 0, orig_h = 0;
        bool high_dpi = false;

        if (fs::exists(wm_size_file)) {
            auto content = SystemHelper::read_file(fs::path(wm_size_file));
            auto xpos = content.find('x');
            if (xpos != std::string::npos) {
                try {
                    int _w = std::stoi(content.substr(0, xpos));
                    int _h = std::stoi(content.substr(xpos + 1));
                    if (_w < _h) {
                        orig_w = _h;
                        orig_h = _w;
                    } else {
                        orig_w = _w;
                        orig_h = _h;
                    }
                } catch (...) {
                }
            }
            if (orig_w >= 2340) high_dpi = true;
        }

        if (orig_w == 0 && cfg_.is_termux) {
            auto result = Executor::shell("wm size 2>/dev/null | grep -oE '[0-9]+x[0-9]+'");
            if (result.ok()) {
                auto xpos = result.stdout_data.find('x');
                if (xpos != std::string::npos) {
                    try {
                        orig_w = std::stoi(result.stdout_data.substr(0, xpos));
                        orig_h = std::stoi(result.stdout_data.substr(xpos + 1));
                    } catch (...) {
                    }
                }
            }
            if (orig_w >= 2340) high_dpi = true;
        }

        if (orig_w > 0 && !cfg_.is_termux) {
            std::string res_str = std::to_string(orig_w) + "x" + std::to_string(orig_h);
            auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(std::string(_("gui.hidpi.resolution_confirm_title")) + res_str)
                .add_arg("--yes-button").add_arg(_("gui.hidpi.yes"))
                .add_arg("--no-button").add_arg(_("gui.hidpi.no"))
                .add_arg("--yesno")
                .add_arg(std::string(_("gui.hidpi.android_resolution_prompt")) + res_str)
                .add_arg("0").add_arg("50")
                .build_string());
            if (r.exit_code != 0) {
                orig_w = 0;
                orig_h = 0;
                high_dpi = false;
            } else {
                Logger::info(std::string(_("gui.hidpi.your_resolution")) + res_str);
            }
        }

        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);

        // 所有桌面统一两档分辨率选择（VNC 配置页面可详细调）
        if (orig_w == 0 && !desktop_manager_.is_auto_install_mode()) {
            auto r2 = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(std::string(_("gui.hidpi.screen_title")))
                .add_arg("--yes-button").add_arg(_("gui.hidpi.res_720p_1080p"))
                .add_arg("--no-button").add_arg(_("gui.hidpi.res_2k_4k"))
                .add_arg("--yesno")
                .add_arg(_("gui.hidpi.screen_prompt"))
                .add_arg("0").add_arg("50")
                .build_string());
            if (r2.exit_code == 0) {
                orig_w = 1920;
                orig_h = 1080;
            } else {
                orig_w = 2560;
                orig_h = 1440;
            }
        }

        if (d.find("lxde") != std::string::npos) {
            for (const auto &f: {"/etc/xdg/autostart/lxpolkit.desktop", "/usr/bin/lxpolkit"}) {
                if (fs::exists(f)) {
                    std::error_code ec;
                    fs::rename(f, std::string(f) + ".bak", ec);
                }
            }
        }

        if (orig_w == 0) {
            orig_w = 1440;
            orig_h = 720;
        }

        if (high_dpi) {
            Logger::info(_("gui.hidpi.auto_adjust"));
            Executor::shell(CommandBuilder("dbus-launch")
                            .add_arg("xfconf-query")
                            .add_arg("-c").add_arg("xsettings")
                            .add_arg("-t").add_arg("int")
                            .add_arg("-np").add_arg("/Gdk/WindowScalingFactor")
                            .add_arg("-s").add_arg("2")
                            .build_string() + " 2>/dev/null || true");
            auto os_rel_content = SystemHelper::read_file("/etc/os-release");
            if (os_rel_content.find("Focal Fossa") != std::string::npos) {
                Executor::shell(CommandBuilder("xfconf-query")
                                .add_arg("-c").add_arg("xfwm4")
                                .add_arg("-p").add_arg("/general/theme")
                                .add_arg("-s").add_arg("Kali-Light-xHiDPI")
                                .build_string() + " 2>/dev/null || true");
            } else {
                Executor::shell(CommandBuilder("xfconf-query")
                                .add_arg("-c").add_arg("xfwm4")
                                .add_arg("-p").add_arg("/general/theme")
                                .add_arg("-s").add_arg("Default-xhdpi")
                                .build_string() + " 2>/dev/null || true");
            }
            Logger::info(std::string(_("gui.hidpi.resolution_adjusted")) + std::to_string(orig_w) + "x" + std::to_string(orig_h) + _("gui.hidpi.scale_2x"));
        }

        if (orig_w > 0) {
            vnc_manager_.config().resolution_w = orig_w;
            vnc_manager_.config().resolution_h = orig_h;
            vnc_manager_.configure_vnc_defaults();

            // 持久化到 ~/.vnc/config（TigerVNC 真正读取的配置文件）
            std::string res_str = std::to_string(orig_w) + "x" + std::to_string(orig_h);
            Executor::shell(
                "mkdir -p " + vnc_manager_.config().vnc_home_dir.string() + " && "
                "grep -q '^geometry=' " + vnc_manager_.config().vnc_home_dir.string() + "/config 2>/dev/null && "
                "sed -i 's/^geometry=.*/geometry=" + res_str + "/' "
                + vnc_manager_.config().vnc_home_dir.string() + "/config || "
                "echo 'geometry=" + res_str + "' >> "
                + vnc_manager_.config().vnc_home_dir.string() + "/config");
        }
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 输入法与浏览器
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::download_wallpaper(std::string_view source) {
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
            Logger::info(std::string(_("gui.wallpaper.unknown_source")) + std::string(source) + _("gui.wallpaper.skip"));
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

    bool GUIManager::install_conky() {
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

    bool GUIManager::install_compiz() {
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

    bool GUIManager::install_cursor_theme(std::string_view theme) {
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

    bool GUIManager::deploy_xfce_panel_config() {
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

    std::string GUIManager::generate_xfce_panel_xml() {
        return gui_config::XFCE_PANEL_XML;
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 2: 额外的内联脚本生成
    // ═══════════════════════════════════════════════════════════════

    void GUIManager::ubuntu_gnome_wallpapers_menu() {
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


    void GUIManager::download_uos_icon_theme() {
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


    void GUIManager::download_deepin_wallpaper() {
        Logger::step(_("gui.wallpaper.deepin_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/deepin/pool/main/d/deepin-wallpapers/";
        Executor::shell("cd /tmp && for GREP_NAME in 'deepin-community-wallpapers' 'deepin-wallpapers_'; do "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep \"$GREP_NAME\" | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o deepin.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv deepin.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null); done || true");
    }


    void GUIManager::xfce_theme_parsing() {
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


    void GUIManager::download_win10x_theme() {
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


    void GUIManager::xubuntu_wallpapers_menu() {
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


    void GUIManager::install_gui() {
        Logger::step(_("gui.install_gui_title"));

        if (cfg_.is_wsl) {
            Logger::info(_("gui.wsl_detected"));
            download_wsl_components();
        }

        const char *iosevka_file = "/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf";
        if (!fs::exists(iosevka_file)) {
            desktop_manager_.download_iosevka_ttf_font_ext();
        }

        check_zstd();
        random_neko();

        std::string lxde_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/BUSYeSLZRqq3i3oM.png"
                                   : "https://gitee.com/ak2/icons/raw/master/raspbian-lxde.jpg";
        std::string mate_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/1frRp1lpOXLPz6mO.jpg"
                                   : "https://gitee.com/ak2/icons/raw/master/ubuntu-mate.jpg";
        std::string xfce_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/a7IQ9NnfgPckuqRt.jpg"
                                   : "https://gitee.com/ak2/icons/raw/master/debian-xfce.jpg";

        if (Executor::has("catimg")) {
            download_and_cat_icon_img(lxde_url, "LXDE_BUSYeSLZRqq3i3oM.png");
            download_and_cat_icon_img(mate_url, "MATE_1frRp1lpOXLPz6mO.jpg");
        }
        catimg_preview_lxde_mate_xfce();

        Logger::info(_("gui.press_enter_select_de"));
        Logger::press_enter();

        run_desktop_install_menu();
    }


    void GUIManager::download_arch_xfce_artwork() {
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


    void GUIManager::download_manjaro_wallpaper() {
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


    void GUIManager::download_candy_icon_theme() {
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


    void GUIManager::choose_vnc_port_5902_or_5903() {
        auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--title").add_arg(_("gui.vnc.port"))
            .add_arg("--yes-button").add_arg("5902")
            .add_arg("--no-button").add_arg("5903")
            .add_arg("--yesno")
            .add_arg(_("gui.vnc.port_select_prompt"))
            .add_arg("0").add_arg("50")
            .build_string());
        if (r.exit_code == 0) {
            vnc_manager_.config().display = 2;
        } else {
            vnc_manager_.config().display = 3;
        }
        vnc_manager_.config().update_port();
        Executor::shell(CommandBuilder("sed")
                        .add_arg("-i")
                        .add_arg("-E")
                        .add_arg("s@(tmoe-linux) :.*@\\1 :" +
                                 std::to_string(vnc_manager_.config().display) + "@")
                        .add_arg("/usr/local/bin/startvnc")
                        .build_string() + " 2>/dev/null || true");
        Executor::shell(CommandBuilder("sed")
                        .add_arg("-i")
                        .add_arg("-E")
                        .add_arg("s@(VNC_DISPLAY)=.*@\\1=" +
                                 std::to_string(vnc_manager_.config().display) + "@")
                        .add_arg("/usr/local/bin/startvnc")
                        .build_string() + " 2>/dev/null || true");
    }


    void GUIManager::check_zstd() {
        if (!Executor::has("zstd")) {
            PackageManager::install("zstd", get_family(cfg_));
        }
    }


    void GUIManager::check_theme_url(std::string &url) {
        if (url.find("www") != std::string::npos && url.find("http") == std::string::npos) {
            url = "https://" + url;
        }
    }


    void GUIManager::download_ubuntu_wallpaper(const std::string &ubuntu_code) {
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


    void GUIManager::random_neko() {
        const std::string nekos[] = {
            _("gui.neko_greeting"),
            "🐱 喵呜~ 今天想安装什么桌面呢?",
            "🐱 喵喵~ 让可爱的猫娘来帮你吧!",
            "🐱 喵~ つまらない時は、デスクトップをインストールしましょう!",
        };
        int i = std::time(nullptr) % 4;
        Logger::info(nekos[i]);
    }


    void GUIManager::catimg_preview_lxde_mate_xfce() {
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


    void GUIManager::tmoe_theme_installer(const std::string &file_path, bool is_icon) {
        std::string extract_path = is_icon ? "/usr/share/icons" : "/usr/share/themes";
        fs::create_directories(extract_path);
        auto tar_cmd = CommandBuilder("tar").add_arg("-xf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        auto tar_j_cmd = CommandBuilder("tar").add_arg("-Jxf").add_arg(file_path)
                .add_arg("-C").add_arg(extract_path).build_string();
        Executor::passthrough(tar_cmd + " 2>/dev/null || " + tar_j_cmd + " 2>/dev/null || true");
        Logger::ok(std::string(_("gui.theme.installed_to")) + extract_path);
    }


    void GUIManager::download_wallpapers_menu() {
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
            else if (ch == "5") download_raspbian_pixel_wallpaper();
            else if (ch == "6") download_manjaro_wallpaper();
            else if (ch == "7") download_debian_gnome_wallpaper();
            else if (ch == "8") download_arch_xfce_artwork();
            else if (ch == "9") download_arch_wallpaper();
            Logger::press_enter();
        }
    }


    void GUIManager::download_icon_themes_menu() {
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
            else if (ch == "3") download_raspbian_pixel_icon_theme();
            else if (ch == "4") download_paper_icon_theme();
            else if (ch == "5") desktop_manager_.download_papirus_icon_theme();
            else if (ch == "6") desktop_manager_.install_numix_theme_ext();
            else if (ch == "7") desktop_manager_.install_moka_theme_ext();
            else if (ch == "8") download_uos_icon_theme();
            Logger::press_enter();
        }
    }


    void GUIManager::configure_mouse_cursor() {
        Logger::info(_("gui.cursor.downloading_modern"));
        download_chameleon_cursor_theme();
    }


    void GUIManager::download_and_cat_icon_img(const std::string &url, const std::string &filename) {
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


    void GUIManager::download_wsl_components() {
        if (!cfg_.is_wsl) return;
        Logger::step(_("gui.wsl_components_download"));

        std::string pub_dl = "/mnt/c/Users/Public/Downloads";
        fs::create_directories(pub_dl + "/pulseaudio");
        fs::create_directories(pub_dl + "/VcXsrv");
        fs::create_directories(pub_dl + "/tigervnc");

        Logger::info("下载 Windows PulseAudio...");
        Executor::shell("cd " + pub_dl + "/pulseaudio && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/pulseaudio/pulseaudio.tar.xz' -o pulseaudio.tar.xz 2>/dev/null && "
                        "tar -Jxvf pulseaudio.tar.xz 2>/dev/null || true");

        Logger::info("下载 Windows VcXsrv X Server...");
        Executor::shell("cd " + pub_dl + "/VcXsrv && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/VcXsrv/VcXsrv.tar.xz' -o VcXsrv.tar.xz 2>/dev/null && "
                        "tar -Jxvf VcXsrv.tar.xz 2>/dev/null || true");

        Logger::info("下载 Windows TigerVNC Viewer...");
        Executor::shell("cd " + pub_dl + "/tigervnc && "
                        "curl -LfsS 'https://gitee.com/ak2/tigervnc-viewer/raw/master/vncviewer64.zip' -o vncviewer64.zip 2>/dev/null && "
                        "unzip -o vncviewer64.zip 2>/dev/null || true");

        Logger::ok(_("gui.wsl_components_done"));
    }


    void GUIManager::set_vnc_passwd() {
        vnc_manager_.configure_vnc_password();
    }


    void GUIManager::fix_vnc_dbus_launch() {
        vnc_manager_.fix_vnc_dbus();
        Logger::ok(_("gui.vnc.dbus_fix_done"));
    }


    void GUIManager::ubuntu_wallpapers_and_photos_menu() {
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


    std::string GUIManager::get_local_ip_addresses() const {
        return vnc_manager_.get_local_ip_addresses();
    }


    void GUIManager::download_debian_gnome_wallpaper() {
        Logger::step(_("gui.wallpaper.debian_gnome_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/g/gnome-backgrounds/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'gnome-backgrounds' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o gnome-bg.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv gnome-bg.deb 2>/dev/null; tar -Jxvf data.tar.xz -C / 2>/dev/null) || true");
    }


    void GUIManager::link_to_debian_wallpaper() {
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


    void GUIManager::download_raspbian_pixel_wallpaper() {
        Logger::step(_("gui.wallpaper.raspbian_download"));
        download_raspbian_pixel_icon_theme();
    }


    void GUIManager::local_theme_installer() {
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


    void GUIManager::download_elementary_wallpaper() {
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


    void GUIManager::download_paper_icon_theme() {
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


    void GUIManager::download_raspbian_pixel_icon_theme() {
        Logger::step(_("gui.icon.raspbian_download_step"));
        std::string repo = "https://mirrors.bfsu.edu.cn/raspberrypi/pool/ui/p/pixel-wallpaper/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'pixel-wallpaper' | grep all.deb | "
                        "tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o pixel.deb '" + repo + "'\"$LATEST\" && "
                        "ar xv pixel.deb && tar -Jxvf data.tar.xz -C / 2>/dev/null || true");
    }


    void GUIManager::download_manjaro_pkg(const std::string &theme_name,
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


    void GUIManager::download_arch_wallpaper() {
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


    void GUIManager::configure_theme_menu() {
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


    void GUIManager::linux_mint_backgrounds_menu() {
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


    void GUIManager::check_tmoe_linux_desktop_link() {
        ensure_tmoe_symlink();
    }

    void GUIManager::ensure_tmoe_symlink() {
        Executor::shell(
            "TOME_BIN=$(readlink -f /proc/self/exe 2>/dev/null || echo /usr/local/bin/tome); "
            "ln -sfv \"$TOME_BIN\" /usr/local/bin/tmoe 2>/dev/null || true");
        Executor::shell(CommandBuilder("ln")
                        .add_arg("-svf")
                        .add_arg("tmoe")
                        .add_arg("/usr/local/bin/tome")
                        .build_string() + " 2>/dev/null || true");
    }


    bool GUIManager::handle_gui_cli_flag(std::string_view flag) {
        if (flag == "--auto-install-gui-xfce") {
            return auto_install_gui("xfce");
        } else if (flag == "--auto-install-gui-lxde") {
            return auto_install_gui("lxde");
        } else if (flag == "--auto-install-gui-lxqt") {
            return auto_install_gui("lxqt");
        } else if (flag == "--auto-install-gui-mate") {
            return auto_install_gui("mate");
        } else if (flag == "--auto-install-gui-kde") {
            return auto_install_gui("kde");
        } else if (flag == "--auto-install-gui-cutefish") {
            return auto_install_gui("cutefish");
        } else if (flag == "--auto-install-gui-dde") {
            return auto_install_gui("dde");
        } else if (flag == "--auto-install-gui-ukui") {
            return auto_install_gui("ukui");
        } else if (flag == "--auto-install-vscode") {
            desktop_manager_.set_auto_install_mode(true);
            desktop_manager_.set_auto_install_flags(false, false, true, true, false, "");
            return auto_install_gui("xfce");
        } else if (flag == "--install-gui" || flag == "install-gui") {
            install_gui();
            return true;
        } else if (flag == "-b") {
            run_beautification_menu();
            return true;
        } else if (flag == "-c") {
            run_remote_desktop_menu();
            return true;
        } else if (flag == "-x") {
            run_xsdl_config_menu();
            return true;
        } else if (flag == "--vncpasswd") {
            set_vnc_passwd();
            return true;
        } else if (flag == "--choose-vnc-port") {
            choose_vnc_port_5902_or_5903();
            return true;
        } else if (flag == "--fix-dbus") {
            fix_vnc_dbus_launch();
            return true;
        } else if (flag == "--start-vnc") {
            vnc_manager_.start_vnc();
            return true;
        } else if (flag == "--stop-vnc") {
            vnc_manager_.stop_vnc();
            return true;
        } else if (flag == "--start-xsdl") {
            start_xsdl();
            return true;
        } else if (flag == "--start-x11vnc") {
            vnc_manager_.start_x11vnc();
            return true;
        } else if (flag == "--start-novnc") {
            remote_desktop_manager_.start_novnc();
            return true;
        }
        install_gui();
        return true;
    }


    void GUIManager::download_chameleon_cursor_theme() {
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


    void GUIManager::download_ubuntu_kylin_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_kylin_download"));
        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntukylin-wallpapers/";
        Executor::shell("cd /tmp && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'ubuntukylin-wallpapers_' | "
                        "grep '.tar.xz' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o ukylin.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf ukylin.tar.xz -C / 2>/dev/null || true");
    }


    void GUIManager::run_remote_desktop_menu() {
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(std::string(_("gui.remote_title")))
                    .add_arg("--menu").add_arg(std::string(_("gui.remote_prompt")))
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("1").add_arg(std::string(_("gui.remote_tightvnc")))
                    .add_arg("2").add_arg(std::string(_("gui.remote_x11vnc")))
                    .add_arg("3").add_arg(std::string(_("gui.remote_xsdl")))
                    .add_arg("4").add_arg(std::string(_("gui.remote_novnc")))
                    .add_arg("5").add_arg(std::string(_("gui.remote_xrdp")))
                    .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                if (!fs::exists("/usr/local/bin/startvnc")) {
                    Logger::warn(_("gui.remote.no_startvnc_warn"));
                    auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                        .add_arg("--yesno")
                        .add_arg(_("gui.remote.no_startvnc_continue"))
                        .add_arg("0").add_arg("0")
                        .build_string());
                    if (r.exit_code != 0) continue;
                }
                auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(_("gui.remote.vnc_config_title"))
                    .add_arg("--yes-button").add_arg(_("gui.remote.resolution_option"))
                    .add_arg("--no-button").add_arg(_("gui.remote.other_option"))
                    .add_arg("--yesno")
                    .add_arg(_("gui.remote.config_prompt"))
                    .add_arg("9").add_arg("50")
                    .build_string());
                if (r.exit_code == 0) {
                    std::string cmd = CommandBuilder(cfg_.tui_bin)
                            .add_arg("--title").add_arg(_("gui.remote.resolution_input_title"))
                            .add_arg("--inputbox")
                            .add_arg(_("gui.remote.resolution_input_prompt"))
                            .add_arg("15").add_arg("50")
                            .add_arg(std::to_string(vnc_manager_.config().resolution_w) + "x" +
                                     std::to_string(vnc_manager_.config().resolution_h))
                            .build_string();
                    std::string val = Executor::tui_select(cmd);
                    auto xpos = val.find('x');
                    if (!val.empty() && xpos != std::string::npos) {
                        vnc_manager_.config().resolution_w = std::stoi(val.substr(0, xpos));
                        vnc_manager_.config().resolution_h = std::stoi(val.substr(xpos + 1));
                        vnc_manager_.configure_vnc_defaults();
                        Logger::ok(std::string(_("gui.remote.resolution_updated")) + val);
                    }
                } else {
                    run_vnc_config_menu();
                }
            } else if (choice == "2") {
                remote_desktop_manager_.run_x11vnc_config_menu();
            } else if (choice == "3") {
                run_xsdl_config_menu();
            } else if (choice == "4") {
                remote_desktop_manager_.run_novnc_config_menu();
            } else if (choice == "5") {
                remote_desktop_manager_.run_xrdp_menu();
            }
            Logger::press_enter();
        }
    }
} // namespace tmoe::domain
