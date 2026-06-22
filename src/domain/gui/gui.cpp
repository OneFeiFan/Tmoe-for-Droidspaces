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
                Logger::info("检测到您处于proot容器环境下，即将为您卸载udisk2和gvfs");
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
                                 ? "检测到桌面的 session 文件, 请选择 tiger！\nPlease choose tiger vncserver！"
                                 : "请选择 startvnc 命令所启动的 VNC 服务端(っ °Д °)\n"
                                 "尽管 tight 可能更加流畅, 但是 tiger 比 tight 支持更多的特效和选项,\n"
                                 "例如鼠标指针和背景透明等\n"
                                 "Although tiger can show more special effects, tight may be smoother.\n"
                                 "It is recommended to use tiger.\n"
                                 "You only need to edit the startvnc script to change the vnc server at any time.";

        auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--title").add_arg("Which vnc server do you prefer")
            .add_arg("--yes-button").add_arg("tiger")
            .add_arg("--no-button").add_arg("tight")
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
        if (!passwd_exists) {
            vnc_manager_.configure_vnc_password();
        }

        // 5d. 选择 VNC 端口
        if (desktop_manager_.is_auto_install_mode()) {
            vnc_manager_.config().display = 2;
            vnc_manager_.config().update_port();
        } else {
            auto port_r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg("VNC PORT")
                .add_arg("--yes-button").add_arg("5902")
                .add_arg("--no-button").add_arg("5903")
                .add_arg("--yesno")
                .add_arg("请选择VNC端口✨\\nPlease choose a vnc port")
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
            Executor::shell(CommandBuilder("sed")
                            .add_arg("-i")
                            .add_arg("-E")
                            .add_arg("s@(geometry)=.*@\\1=" + res_str + "@")
                            .add_arg("/etc/tigervnc/vncserver-config-tmoe")
                            .build_string() + " 2>/dev/null || true");
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
            Executor::passthrough(CommandBuilder("chown")
                                  .add_arg("-R")
                                  .add_arg("$(id -un):$(id -gn)")
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
        Logger::info("一：关于音频服务无法自动启动的说明：");
        Logger::info("若发现启动vnc后无法连接到音频服务，请新建termux会话并输pulseaudio --start。");
        Logger::info("若您的音频服务端为Android系统，请在图形界面启动完成后输pulseaudio -D来启动音频服务后台进程。");
        Logger::info("若您的音频服务端为windows10系统，则请手动打开C:\\Users\\Public\\Downloads\\pulseaudio\\pulseaudio.bat。");
        Logger::info("------------------------");
        Logger::info("二：关于VNC和X的启动说明");
        Logger::info("您之后可以在原系统里输startvnc同时启动vnc服务端和客户端。");
        Logger::info("在容器里输startvnc启动vnc服务端，输stopvnc停止");
        Logger::info("You can type startvnc to start vncserver, type stopvnc to stop it.");
        Logger::info("You can also type startxsdl to start X client and server.");
        Logger::info("在原系统里输startxsdl同时启动X客户端与服务端");
        Logger::info("------------------------");
        Logger::info("关于音频服务: 若宿主机为Android, 输 pulseaudio --start。");
        Logger::info("若宿主机为Win10, 手动运行 C:\\Users\\Public\\Downloads\\pulseaudio\\pulseaudio.bat。");
        if (Executor::has("apt-get")) {
            Logger::info("输入tightvnc启动tightvnc服务端，输入tigervnc启动tigervnc服务端。");
        }
        Logger::info("tightvnc+tigervnc & x window配置完成,将为您配置x11vnc");
        Logger::info("------------------------");

        Logger::info(std::string(_("gui.section_three")) + "：");
        vnc_manager_.check_xvnc_command();
        remote_desktop_manager_.x11vnc_warning();
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
        Logger::info("注：配置完本工具所支持的所有VNC,将解锁成就*^^*");
        remote_desktop_manager_.do_you_want_to_configure_novnc();
    }

    bool GUIManager::configure_xsdl() {
        Logger::step("配置 XSDL 连接...");

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

        Logger::ok("XSDL DISPLAY 设置为: " + display);
        return true;
    }

    bool GUIManager::start_xsdl() {
        Logger::step("启动 XSDL/VcXsrv 模式...");

        if (cfg_.is_wsl) {
            vnc_manager_.detect_wsl_environment();
        }

        std::string display = cfg_.is_wsl ? (vnc_manager_.config().windows_ip + ":0") : "127.0.0.1:0";
        std::string env = "export DISPLAY=" + display + " "
                          "export PULSE_SERVER=tcp:" + display.substr(0, display.find(':')) + ":" +
                          std::to_string(vnc_manager_.config().pulse_port);

        if (cfg_.is_wsl) {
            Logger::info("检测到 WSL 环境，请在 Windows 上启动 VcXsrv");
            Logger::info("DISPLAY=" + display);
        }

        std::string cmd = env + " xfce4-session 2>/dev/null || " + env + " startxfce4 2>/dev/null || "
                          + env + " openbox 2>/dev/null &";
        Executor::passthrough(cmd);

        Logger::ok("XSDL 模式已启动 — DISPLAY=" + display);
        return true;
    }

    bool GUIManager::configure_wsl_pulseaudio() {
        Logger::step("配置 WSL PulseAudio...");

        if (!cfg_.is_wsl) {
            Logger::warn("当前不在 WSL 环境中");
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

        Logger::ok("WSL PulseAudio 已配置: " + pulse_server);
        Logger::info("请在 Windows 端启动 PulseAudio 服务");
        return true;
    }

    bool GUIManager::start_wslg(int display_port) {
        Logger::step("启动 WSLg (Xwayland)...");

        if (!cfg_.is_wsl) {
            Logger::warn("WSLg 仅适用于 Win11 WSL2 环境");
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

        Logger::ok("WSLg Xwayland 已启动 — DISPLAY=:" + std::to_string(display_port));
        Logger::info("现在可以运行 GUI 程序，画面将显示在 Windows 宿主机上");
        return true;
    }

    bool GUIManager::beautify_desktop() {
        Logger::step("桌面美化...");
        return true;
    }

    bool GUIManager::install_theme(std::string_view theme) {
        Logger::step("安装桌面主题: " + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool GUIManager::install_icon_theme(std::string_view theme) {
        Logger::step("安装图标主题: " + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::icon_theme_package_name(name_lower);
        return PackageManager::install(pkg_name, get_family(cfg_));
    }

    bool GUIManager::set_wallpaper(std::string_view path) {
        Logger::step("设置壁纸...");

        if (!path.empty() && fs::exists(path)) {
            Executor::shell(CommandBuilder("xfconf-query")
                            .add_arg("-c").add_arg("xfce4-desktop")
                            .add_arg("-p").add_arg("/backdrop/screen0/monitor0/workspace0/last-image")
                            .add_arg("-s").add_arg(std::string(path))
                            .build_string() + " 2>/dev/null &");
            Logger::ok("壁纸已设置");
            return true;
        }

        Logger::info("可使用以下命令手动设置壁纸:");
        Logger::info(
            "  xfconf-query -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image -s /path/to/wallpaper.jpg");
        return true;
    }

    bool GUIManager::install_dock() {
        Logger::step("安装 Plank dock...");
        return PackageManager::install("plank", get_family(cfg_));
    }

    // ═══════════════════════════════════════════════════════════════
    // PulseAudio
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::start_pulseaudio_bridge() const {
        if (cfg_.is_wsl) {
            Logger::info("WSL 环境: PulseAudio 由 Windows 宿主机提供");
            Logger::info("若音频未启动，请手动打开 C:\\Users\\Public\\Downloads\\pulseaudio\\pulseaudio.bat");
            return true;
        }

        if (!Executor::has("pulseaudio")) {
            Logger::warn("宿主机未安装 PulseAudio，将跳过声音桥接初始化");
            return false;
        }

        Logger::step("正在拉起 PulseAudio TCP 桥接守护进程...");

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
                    .add_arg("--title").add_arg("Modify vnc server conf")
                    .add_arg("--menu")
                    .add_arg("Type startvnc to start vncserver,输入startvnc启动vnc服务")
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("edit_startvnc").add_arg("Edit startvnc 编辑vnc启动脚本")
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
                    .add_arg("8").add_arg("Edit xsession")
                    .add_arg("9").add_arg("Edit tigervnc-config")
                    .add_arg("10").add_arg("Window scaling factor")
                    .add_arg("11").add_arg("WSL pulseaudio(only for windows)")
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
                        .add_arg("--title").add_arg("MODIFY PULSE SERVER ADDRESS")
                        .add_arg("--inputbox")
                        .add_arg("输入 PulseAudio 服务器地址, linux 默认为 127.0.0.1, WSL2 为宿主机 ip\\\\n"
                            "Android-Termux 需输 pulseaudio --start\\\\n"
                            "例如 192.168.1.3:4713")
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
                    Logger::ok("PULSE_SERVER 已更新为: " + pa_addr);
                }
            } else if (choice == "8") {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/X11/xinit/Xsession");
            } else if (choice == "9") {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/tigervnc/vncserver-config-tmoe");
            } else if (choice == "10") {
                std::string scale_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg("Window scaling factor")
                        .add_arg("--inputbox")
                        .add_arg("请选择缩放因子 (1 或 2)\\n1 = 正常, 2 = 2倍缩放\\n"
                            "1 = normal, 2 = HiDPI")
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
                            Logger::info("Focal Fossa: 已设置主题为 Kali-Light-xHiDPI");
                        }
                    }
                    Logger::ok("WindowScalingFactor 已设置为 " + scale);
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
                    .add_arg("--menu").add_arg(" ")  // 空字符串会被 add_arg 跳过,用空格代替
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
                    .add_arg("--menu").add_arg(" ")  // 空字符串会被 add_arg 跳过,用空格代替
                    .add_arg("0").add_arg("0").add_arg("0");
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root) {
                    menu_cmd.add_arg(std::to_string(idx++)).add_arg(d.name + " (" + d.compat_notes + ")");
                }
            }
            menu_cmd.add_arg("0").add_arg(_("menu.tui.back"));
            auto ch = Executor::tui_select(menu_cmd.build_string());
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_desktop(d.id);
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
                    .add_arg("--menu").add_arg(" ")  // 空字符串会被 add_arg 跳过,用空格代替
                    .add_arg("0").add_arg("0").add_arg("0");
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.requires_root) {
                    menu_cmd.add_arg(std::to_string(idx++)).add_arg(d.name + " (" + d.compat_notes + ")");
                }
            }
            menu_cmd.add_arg("0").add_arg(_("menu.tui.back"));
            auto ch = Executor::tui_select(menu_cmd.build_string());
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.requires_root) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_desktop(d.id);
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
                        if (d.id == "fvwm") {
                            desktop_manager_.install_fvwm_ext();
                        } else {
                            desktop_manager_.install_window_manager(d.id);
                        }
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
                .add_arg("1").add_arg("lightdm: 支持跨桌面,可使用各种前端")
                .add_arg("2").add_arg("sddm: 现代化DM,替代KDE4的KDM")
                .add_arg("3").add_arg("gdm: GNOME默认DM")
                .add_arg("4").add_arg("slim: Lightweight轻量")
                .add_arg("5").add_arg("lxdm: LXDE默认DM(独立于桌面环境)")
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
                    .add_arg("--title").add_arg("Modify x server conf")
                    .add_arg("--menu")
                    .add_arg("Type startxsdl to start x11.输startxsdl启动x11")
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg("Pulse server port 音频端口")
                    .add_arg("2").add_arg("Display number 显示编号")
                    .add_arg("3").add_arg("ip address")
                    .add_arg("4").add_arg("Edit manually 手动编辑")
                    .add_arg("5").add_arg("DISPLAY switch 转发显示开关(仅qemu)")
                    .add_arg("6").add_arg("VcXsrv显示端口(仅win10)")
                    .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                    .build_string();
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            std::string script = "/usr/local/bin/startxsdl";
            if (ch == "1") {
                std::string cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg("MODIFY PULSE SERVER PORT")
                        .add_arg("--inputbox").add_arg("Please type the pulse server port")
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
                        .add_arg("--title").add_arg("DISPLAY NUMBER")
                        .add_arg("--inputbox").add_arg("请输入显示编号 (默认0)")
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
                        .add_arg("--title").add_arg("IP ADDRESS")
                        .add_arg("--inputbox").add_arg("请输入 ip 地址 (默认 127.0.0.1)")
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
                        .add_arg("--yesno").add_arg("是否切换 DISPLAY 转发开关？")
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
                        .add_arg("--title").add_arg("VcXsrv DISPLAY PORT")
                        .add_arg("--inputbox").add_arg("请输入 VcXsrv 显示端口 (默认 :0)")
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
        while (true) {
            std::string menu_cmd = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg(std::string(_("gui.beautify_title")))
                    .add_arg("--menu").add_arg(std::string(_("gui.beautify_prompt")))
                    .add_arg("0").add_arg("0").add_arg("0")
                    .add_arg("1").add_arg(std::string(_("gui.beautify_gtk_theme")))
                    .add_arg("2").add_arg(std::string(_("gui.beautify_icon_theme")))
                    .add_arg("3").add_arg(std::string(_("gui.beautify_wallpaper")))
                    .add_arg("4").add_arg(std::string(_("gui.beautify_plank")))
                    .add_arg("5").add_arg(std::string(_("gui.beautify_compiz")))
                    .add_arg("6").add_arg(std::string(_("gui.beautify_conky")))
                    .add_arg("7").add_arg(std::string(_("gui.beautify_cursor")))
                    .add_arg("8").add_arg(std::string(_("gui.beautify_xfce_panel")))
                    .add_arg("9").add_arg(std::string(_("gui.beautify_iosevka")))
                    .add_arg("0").add_arg(std::string(_("menu.tui.back")))
                    .build_string();

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                std::string theme_menu = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.gtk_theme_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.gtk_theme_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("arc").add_arg(std::string(_("gui.gtk_theme_arc")))
                        .add_arg("adapta").add_arg(std::string(_("gui.gtk_theme_adapta")))
                        .add_arg("numix").add_arg(std::string(_("gui.gtk_theme_numix")))
                        .add_arg("materia").add_arg(std::string(_("gui.gtk_theme_materia")))
                        .add_arg("breeze").add_arg(std::string(_("gui.gtk_theme_breeze")))
                        .build_string();
                std::string t = Executor::tui_select(theme_menu);
                if (!t.empty() && t != "0") install_theme(t);
            } else if (choice == "2") {
                std::string icon_menu = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.icon_theme_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.icon_theme_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("papirus").add_arg(std::string(_("gui.icon_papirus")))
                        .add_arg("numix").add_arg(std::string(_("gui.icon_numix")))
                        .add_arg("breeze").add_arg(std::string(_("gui.icon_breeze")))
                        .add_arg("elementary").add_arg(std::string(_("gui.icon_elementary")))
                        .add_arg("tango").add_arg(std::string(_("gui.icon_tango")))
                        .add_arg("moka").add_arg(std::string(_("gui.icon_moka")))
                        .add_arg("faenza").add_arg(std::string(_("gui.icon_faenza")))
                        .build_string();
                std::string i = Executor::tui_select(icon_menu);
                if (!i.empty() && i != "0") install_icon_theme(i);
            } else if (choice == "3") {
                std::string wp_menu = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.wallpaper_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.wallpaper_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("gnome").add_arg(std::string(_("gui.wp_gnome")))
                        .add_arg("xfce").add_arg(std::string(_("gui.wp_xfce")))
                        .add_arg("mate").add_arg(std::string(_("gui.wp_mate")))
                        .add_arg("deepin").add_arg(std::string(_("gui.wp_deepin")))
                        .add_arg("kde").add_arg(std::string(_("gui.wp_kde")))
                        .build_string();
                std::string wp = Executor::tui_select(wp_menu);
                if (!wp.empty() && wp != "0") download_wallpaper(wp);
            } else if (choice == "4") {
                install_dock();
            } else if (choice == "5") {
                install_compiz();
            } else if (choice == "6") {
                install_conky();
            } else if (choice == "7") {
                std::string cursor_menu = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.cursor_title")))
                        .add_arg("--menu").add_arg(std::string(_("gui.cursor_prompt")))
                        .add_arg("0").add_arg("0").add_arg("0")
                        .add_arg("breeze").add_arg(std::string(_("gui.cursor_breeze")))
                        .add_arg("chameleon").add_arg(std::string(_("gui.cursor_chameleon")))
                        .build_string();
                std::string c = Executor::tui_select(cursor_menu);
                if (!c.empty() && c != "0") install_cursor_theme(c);
            } else if (choice == "8") {
                deploy_xfce_panel_config();
            } else if (choice == "9") {
                install_iosevka_font();
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
            Logger::error("桌面环境安装失败");
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
        Logger::step("安装 Iosevka 编程字体...");

        if (fs::exists("/usr/share/fonts/truetype/iosevka") ||
            fs::exists("/usr/share/fonts/iosevka") ||
            fs::exists("/usr/local/share/fonts/iosevka")) {
            Logger::ok("Iosevka 字体已安装");
            return true;
        }

        auto family = get_family(cfg_);
        if (PackageManager::install("fonts-iosevka", family)) {
            Logger::ok("Iosevka 字体安装完成");
            return true;
        }

        const char *iosevka_url = "https://github.com/be5invis/Iosevka/releases/download/v28.1.0/"
                "PkgTTF-Iosevka-28.1.0.zip";

        std::string font_dir = "/usr/local/share/fonts/iosevka";
        fs::create_directories(font_dir);

        Logger::info("从 GitHub 下载 Iosevka...");
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
            Logger::ok("Iosevka 字体安装完成 -> " + font_dir);
            return true;
        }

        Logger::warn("Iosevka 下载失败，请手动安装: apt install fonts-iosevka");
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
                .add_arg("--title").add_arg("Is your resolution " + res_str + "?")
                .add_arg("--yes-button").add_arg("YES")
                .add_arg("--no-button").add_arg("NO")
                .add_arg("--yesno")
                .add_arg("Your host is detected as Android and the resolution is " + res_str)
                .add_arg("0").add_arg("50")
                .build_string());
            if (r.exit_code != 0) {
                orig_w = 0;
                orig_h = 0;
                high_dpi = false;
            } else {
                Logger::info("Your resolution is " + res_str);
            }
        }

        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (orig_w == 0 && (d.find("xfce") != std::string::npos)) {
            auto r2 = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg("720P/1080P Screen/Monitor")
                .add_arg("--yes-button").add_arg("Yes (720P~1080P)")
                .add_arg("--no-button").add_arg("No (2K~4K)")
                .add_arg("--yesno")
                .add_arg("若屏幕分辨率 x, 720P<=x<=1080P 选 Yes, 2K<=x<=4K 选 No")
                .add_arg("0").add_arg("50")
                .build_string());
            if (r2.exit_code == 0) {
                orig_w = 1440;
                orig_h = 720;
                high_dpi = false;
            } else {
                orig_w = 2880;
                orig_h = 1440;
                high_dpi = true;
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
            Logger::info("Tmoe-linux tool将为您自动调整高分屏设定");
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
            Logger::info("已将默认分辨率修改为" + std::to_string(orig_w) + "x" + std::to_string(orig_h) +
                         "，窗口缩放大小调整为2x");
        }

        vnc_manager_.config().resolution_w = orig_w;
        vnc_manager_.config().resolution_h = orig_h;
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 输入法与浏览器
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::download_wallpaper(std::string_view source) {
        Logger::step("下载壁纸: " + std::string(source));

        std::string wallpaper_dir = "/usr/share/backgrounds/tmoe";
        fs::create_directories(wallpaper_dir);

        std::string url;
        std::string filename;
        std::string src_lower(source);
        std::transform(src_lower.begin(), src_lower.end(), src_lower.begin(), ::tolower);
        auto fam = get_family(cfg_);

        if (src_lower == "debian" || src_lower == "gnome") {
            PackageManager::install("gnome-backgrounds", fam);
            Logger::ok("GNOME 壁纸包已安装");
            return true;
        } else if (src_lower == "xfce" || src_lower == "xubuntu") {
            url = "https://gitlab.xfce.org/artwork/xfce4-artwork/-/raw/master/backgrounds/xfce-stripes.png";
            filename = "xfce-stripes.png";
        } else if (src_lower == "mate" || src_lower == "ubuntu-mate") {
            PackageManager::install("ubuntu-mate-wallpapers", fam);
            Logger::ok("Ubuntu MATE 壁纸包已安装");
            return true;
        } else if (src_lower == "deepin") {
            PackageManager::install("deepin-wallpapers", fam);
            Logger::ok("Deepin 壁纸包已安装");
            return true;
        } else if (src_lower == "kde") {
            PackageManager::install("plasma-workspace-wallpapers", fam);
            Logger::ok("KDE 壁纸包已安装");
            return true;
        } else {
            Logger::info("未知壁纸源: " + std::string(source) + "，跳过下载");
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

        Logger::ok("壁纸下载完成");
        return true;
    }

    bool GUIManager::install_conky() {
        Logger::step("安装 Conky 系统监控...");
        if (!PackageManager::install({"conky", "conky-all"}, get_family(cfg_))) {
            Logger::warn("Conky 安装失败");
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

        Logger::ok("Conky 安装完成");

        if (!fs::exists(home + "/github/Harmattan")) {
            fs::create_directories(home + "/github");
            Executor::shell("cd " + home + "/github && "
                            "(git clone --depth=1 https://github.com/zagortenay333/Harmattan.git 2>/dev/null || "
                            "git clone --depth=1 git://github.com/zagortenay333/Harmattan.git 2>/dev/null || true)");
            if (fs::exists(home + "/github/Harmattan")) {
                Logger::info("Harmattan conky 主题已下载到 " + home + "/github/Harmattan");
                Logger::info("进入目录执行 bash preview 预览效果");
            }
        }

        return true;
    }

    bool GUIManager::install_compiz() {
        Logger::step("安装 Compiz 窗口特效...");
        std::vector<std::string> pkgs = {
            "compiz", "compiz-core", "compiz-plugins",
            "compiz-plugins-default", "compiz-plugins-extra",
            "emerald", "emerald-themes", "compizconfig-settings-manager"
        };

        if (!PackageManager::install(pkgs, get_family(cfg_))) {
            Logger::warn("部分 Compiz 组件安装失败，尝试核心包...");
            PackageManager::install({"compiz", "compiz-core", "compiz-plugins"}, get_family(cfg_));
        }

        Logger::ok("Compiz 安装完成");
        Logger::info("使用 emerald --replace 启用 Emerald 窗口装饰器");
        Logger::info("使用 ccsm 配置 Compiz 特效");
        return true;
    }

    bool GUIManager::install_cursor_theme(std::string_view theme) {
        Logger::step("安装鼠标指针主题: " + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::cursor_theme_package_name(name_lower);
        if (!PackageManager::install(pkg_name, get_family(cfg_))) {
            Logger::warn("鼠标指针安装可能失败，请手动检查");
            return false;
        }
        Logger::ok("鼠标指针主题安装完成");
        return true;
    }

    bool GUIManager::deploy_xfce_panel_config() {
        Logger::step("部署 XFCE4 面板配置...");

        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        fs::path panel_dir = fs::path(home) / ".config" / "xfce4" / "xfconf" / "xfce-perchannel-xml";
        fs::path panel_file = panel_dir / "xfce4-panel.xml";

        if (fs::exists(panel_file)) {
            fs::path backup_dest = fs::path(home) / ".config" / "tmoe-linux" / "xfce4-panel.xml.bak";
            try {
                fs::create_directories(backup_dest.parent_path());
                fs::copy_file(panel_file, backup_dest, fs::copy_options::overwrite_existing);
                Logger::info("已备份 xfce4-panel.xml -> " + backup_dest.string());
            } catch (const fs::filesystem_error &e) {
                Logger::warn("备份 xfce4-panel.xml 失败: " + std::string(e.what()));
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
        menu.add_arg("--title").add_arg("UBUNTU壁纸")
                .add_arg("--menu").add_arg("Download ubuntu wallpaper-packs")
                .add_arg("0").add_arg("50").add_arg("0");
        for (int i = 0; codes[i]; ++i)
            menu.add_arg(std::to_string(i + 1)).add_arg(codes[i]);
        menu.add_arg("0").add_arg("Back");
        auto ch = Executor::tui_select(menu.build_string());
        if (ch == "0" || ch.empty()) return;
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < 22) download_ubuntu_wallpaper(codes[idx]);
    }


    void GUIManager::download_uos_icon_theme() {
        PackageManager::install("deepin-icon-theme", get_family(cfg_));
        if (fs::exists("/usr/share/icons/Uos")) {
            Logger::info("检测到已安装 UOS 图标主题");
            return;
        }
        Logger::step("下载 UOS 图标主题...");
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
        Logger::step("下载 Deepin 壁纸...");
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
                .add_arg("--title").add_arg("Tmoe xfce&gnome theme parser")
                .add_arg("--inputbox")
                .add_arg("请输入主题链接Please enter a url\\n例如https://gnome-look.org/xx或https://xfce-look.org/xx")
                .add_arg("0").add_arg("50")
                .build_string();
        std::string url = Executor::tui_select(url_cmd);
        if (url.empty()) return;

        Logger::info("正在解析主题页面...");
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
            Logger::info("未找到主题文件，请检查URL是否正确");
            return;
        }

        Logger::info("找到以下主题:");
        std::string themes = themes_result.stdout_data;
        std::istringstream iss(themes);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) Logger::info("  " + line);
        }

        Logger::info("请手动选择下载的主题文件名，然后使用本地主题安装器安装");
    }


    void GUIManager::download_win10x_theme() {
        if (fs::exists("/usr/share/icons/We10X-Valley-dark")) {
            Logger::info("检测到已安装 Win10x 图标主题");
            return;
        }
        Logger::step("下载 Win10x 图标主题...");
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
        menu.add_arg("--title").add_arg("桌面壁纸")
                .add_arg("--menu").add_arg("您想要下载哪套xubuntu壁纸包？")
                .add_arg("0").add_arg("50").add_arg("0");
        for (int i = 0; items[i]; ++i)
            menu.add_arg(std::to_string(i + 1)).add_arg(items[i]);
        menu.add_arg("0").add_arg("Back");
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
        Logger::step("下载 Arch XFCE artwork...");
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
        Logger::step("下载 Manjaro 壁纸...");
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
            Logger::info("检测到已安装 candy 图标主题");
            return;
        }
        Logger::step("下载 Candy 图标主题...");
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
            .add_arg("--title").add_arg("VNC PORT")
            .add_arg("--yes-button").add_arg("5902")
            .add_arg("--no-button").add_arg("5903")
            .add_arg("--yesno")
            .add_arg("请选择VNC端口✨\\nPlease choose a vnc port")
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
        Logger::step("下载 Ubuntu 壁纸: " + ubuntu_code);
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
        Logger::ok("主题已安装到 " + extract_path);
    }


    void GUIManager::download_wallpapers_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg("桌面壁纸")
                    .add_arg("--menu")
                    .add_arg("您想要下载哪套壁纸包？\\n Which wallpaper-pack do you want to download? ")
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg("ubuntu:汇聚了官方及社区的绝赞壁纸包")
                    .add_arg("2").add_arg("Mint:聆听自然的律动与风之呼吸,感受清新而唯美")
                    .add_arg("3").add_arg("deepin-community+official 深度")
                    .add_arg("4").add_arg("elementary(如沐春风)")
                    .add_arg("5").add_arg("raspberrypi pixel树莓派(美如画卷)")
                    .add_arg("6").add_arg("manjaro-2017+2018")
                    .add_arg("7").add_arg("gnome-backgrounds(简单而纯粹)")
                    .add_arg("8").add_arg("xfce-artwork")
                    .add_arg("9").add_arg("arch(领略别样艺术)")
                    .add_arg("0").add_arg("🌚 Return to previous menu 返回上级菜单")
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
                    .add_arg("--title").add_arg("图标包")
                    .add_arg("--menu")
                    .add_arg("您想要下载哪个图标包？\\n Which icon-theme do you want to download? ")
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg("win11:更新颖的UI设计")
                    .add_arg("2").add_arg("candy:Sweet gradient icons")
                    .add_arg("3").add_arg("pixel:raspberrypi树莓派")
                    .add_arg("4").add_arg("paper:简约、灵动、现代化的图标包")
                    .add_arg("5").add_arg("papirus:优雅的图标包,基于paper")
                    .add_arg("6").add_arg("numix:modern现代化")
                    .add_arg("7").add_arg("moka:简约一致的美学")
                    .add_arg("8").add_arg("UOS")
                    .add_arg("0").add_arg("🌚 Return to previous menu 返回上级菜单")
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
        Logger::info("正在下载现代化鼠标指针主题...");
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
        Logger::ok("VNC dbus 修复完成");
    }


    void GUIManager::ubuntu_wallpapers_and_photos_menu() {
        while (true) {
            std::string menu = CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg("Ubuntu壁纸包")
                    .add_arg("--menu").add_arg("您想要下载哪套Ubuntu壁纸包？")
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg("ubuntu-gnome:(bionic,cosmic,etc.)")
                    .add_arg("2").add_arg("xubuntu-community:(bionic,focal,etc.)")
                    .add_arg("3").add_arg("ubuntu-mate")
                    .add_arg("4").add_arg("ubuntu-kylin 优麒麟")
                    .add_arg("0").add_arg("Back")
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
        Logger::step("下载 Debian GNOME 壁纸...");
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
        Logger::step("下载 Raspbian Pixel 壁纸...");
        download_raspbian_pixel_icon_theme();
    }


    void GUIManager::local_theme_installer() {
        Logger::info("本地主题安装器");
        Logger::info("请将主题包 (.tar.gz / .tar.xz) 放置在 /tmp 目录");

        std::string cmd = CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg("LOCAL THEME INSTALLER")
                .add_arg("--inputbox")
                .add_arg("请输入主题包文件名 (在 /tmp 目录下)\\n例如: my-theme.tar.xz")
                .add_arg("10").add_arg("60")
                .build_string();
        std::string filename = Executor::tui_select(cmd);

        if (filename.empty()) return;

        std::string full_path = "/tmp/" + filename;
        if (!fs::exists(full_path)) {
            Logger::error("文件不存在: " + full_path);
            return;
        }

        auto type_r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
            .add_arg("--title").add_arg("Please choose the file type")
            .add_arg("--yes-button").add_arg("THEME主题")
            .add_arg("--no-button").add_arg("ICON图标包")
            .add_arg("--yesno").add_arg("这是主题包还是图标包?")
            .add_arg("0").add_arg("50")
            .build_string());
        bool is_icon = (type_r.exit_code == 1);

        tmoe_theme_installer(full_path, is_icon);
    }


    void GUIManager::download_elementary_wallpaper() {
        Logger::step("下载 Elementary 壁纸...");
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
        Logger::step("下载 Paper 图标主题...");
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
        Logger::step("下载 Raspbian Pixel 壁纸/图标...");
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
        Logger::step("下载 Arch 壁纸...");
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
                    .add_arg("--title").add_arg("桌面环境主题")
                    .add_arg("--menu")
                    .add_arg("您想要下载哪个主题？\\n Which theme do you want to download? ")
                    .add_arg("0").add_arg("50").add_arg("0")
                    .add_arg("1").add_arg("🌈 XFCE-LOOK-parser 主题链接解析器")
                    .add_arg("2").add_arg("⚡ local-theme-installer 本地主题安装器")
                    .add_arg("3").add_arg("🎭 win10:kali卧底模式主题")
                    .add_arg("4").add_arg("🚥 MacOS:Mojave")
                    .add_arg("5").add_arg("🍎 MacOS:Big Sur")
                    .add_arg("6").add_arg("🎋 breeze:plasma桌面微风gtk+版主题")
                    .add_arg("7").add_arg("Kali:Flat-Remix-Blue主题")
                    .add_arg("8").add_arg("ukui:国产优麒麟ukui桌面主题")
                    .add_arg("9").add_arg("arc:融合透明元素的平面主题")
                    .add_arg("0").add_arg("🌚 Return to previous menu 返回上级菜单")
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
            menu.add_arg("--title").add_arg("MINT壁纸包")
                    .add_arg("--menu").add_arg("Download Mint wallpaper-packs")
                    .add_arg("0").add_arg("50").add_arg("0");
            for (int i = 0; codes[i]; ++i)
                menu.add_arg(std::to_string(i + 1)).add_arg(codes[i]);
            menu.add_arg("0").add_arg("Back");
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
        Logger::step("下载 Ubuntu Kylin 壁纸...");
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
                    Logger::warn("未检测到 startvnc，您可能尚未安装图形桌面");
                    auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                        .add_arg("--yesno")
                        .add_arg("未检测到 startvnc，是否继续编辑配置？")
                        .add_arg("0").add_arg("0")
                        .build_string());
                    if (r.exit_code != 0) continue;
                }
                auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                    .add_arg("--title").add_arg("modify vnc configuration")
                    .add_arg("--yes-button").add_arg("分辨率 resolution")
                    .add_arg("--no-button").add_arg("其它 other")
                    .add_arg("--yesno")
                    .add_arg("Which configuration do you want to modify?")
                    .add_arg("9").add_arg("50")
                    .build_string());
                if (r.exit_code == 0) {
                    std::string cmd = CommandBuilder(cfg_.tui_bin)
                            .add_arg("--title").add_arg("请输入分辨率")
                            .add_arg("--inputbox")
                            .add_arg("例如 1920x1080, 1440x720, 1280x1024")
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
                        Logger::ok("分辨率已修改为 " + val);
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
