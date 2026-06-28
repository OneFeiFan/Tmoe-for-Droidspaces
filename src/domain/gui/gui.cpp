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
          remote_desktop_manager_(cfg, vnc_manager_, desktop_manager_),
          beautification_manager_(cfg, desktop_manager_) {
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
        auto info = desktop_manager_.get_desktop_info(desktop);
        bool recommend_tiger = info.recommends_tiger_vnc;

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

        // 5c. 设置 VNC 密码（每次安装桌面都必须设定）
        vnc_manager_.configure_vnc_password();

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
                beautification_manager_.run_beautification_menu();
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
            Logger::info(
                std::string(_("gui.hidpi.resolution_adjusted")) + std::to_string(orig_w) + "x" + std::to_string(orig_h)
                + _("gui.hidpi.scale_2x"));
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
            beautification_manager_.download_and_cat_icon_img(lxde_url, "LXDE_BUSYeSLZRqq3i3oM.png");
            beautification_manager_.download_and_cat_icon_img(mate_url, "MATE_1frRp1lpOXLPz6mO.jpg");
        }
        beautification_manager_.catimg_preview_lxde_mate_xfce();

        Logger::info(_("gui.press_enter_select_de"));
        Logger::press_enter();

        run_desktop_install_menu();
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


    // ═══════════════════════════════════════════════════════════════
    // Phase 7: x11vnc / XRDP 增强
    // ═══════════════════════════════════════════════════════════════


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


    std::string GUIManager::get_local_ip_addresses() const {
        return vnc_manager_.get_local_ip_addresses();
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
            beautification_manager_.run_beautification_menu();
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


    // ═══════════════════════════════════════════════════════════════
    // Phase 6: WSL 完整支持
    // ═══════════════════════════════════════════════════════════════


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
