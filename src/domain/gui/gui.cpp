#include "gui.hpp"
#include "core/str_utils.h"
#include "ui/plugin_helpers.h"
#include "ui/menu_engine.h"
#include "ui/menus/gui_desktop_plugin.h"
#include "ui/menus/plugin_factories.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    namespace {
        DistroFamily get_family(const TmoeConfig &cfg) {
            return PackageManager::resolve_family(cfg.linux_distro);
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
            bool is_old_distro = contains(os_rel_content, "Focal Fossa") ||
                                 contains(os_rel_content, "focal") ||
                                 contains(os_rel_content, "bionic") ||
                                 contains(os_rel_content, "Bionic Beaver") ||
                                 contains(os_rel_content, "Eoan Ermine") ||
                                 contains(os_rel_content, "buster") ||
                                 contains(os_rel_content, "stretch") ||
                                 contains(os_rel_content, "jessie") ||
                                 contains(os_rel_content, "Deepin 20") ||
                                 contains(os_rel_content, "Uos 20");
            if (is_old_distro) {
                Logger::info(_("gui.vnc.proot_remove_udisks"));
                Executor::passthrough("sudo apt purge -y --allow-change-held-packages ^udisks2 ^gvfs 2>/dev/null || true");
            }
        }

        // dpkg 修复
        if (Executor::has("apt-get")) {
            Executor::passthrough("sudo dpkg --configure -a 2>/dev/null || true");
        }

        vnc_manager_.configure_startvnc();
        vnc_manager_.configure_startxsdl();
        Executor::shell(CommandBuilder("sudo").add_arg("chmod")
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

        // 5b. VNC 服务端：Bash 仅在交互模式 + debian 下弹 whiptail，否则默认 tiger
        auto info = desktop_manager_.get_desktop_info(desktop);
        family = get_family(cfg_);
        bool interactive = !desktop_manager_.is_auto_install_mode();
        if (interactive && family == DistroFamily::Debian) {
            // Bash: which_vnc_server_do_you_prefer → 非 auto 模式 + debian 才弹 whiptail
            std::string prompt = info.recommends_tiger_vnc
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
            } else {
                vnc_manager_.config().server = "tight";
                vnc_manager_.config().server_bin = "tightvnc";
            }
        } else {
            // Auto / 非debian: 默认 tiger（Bash 原版行为）
            vnc_manager_.config().server = "tiger";
            vnc_manager_.config().server_bin = "tigervnc";
        }

        vnc_manager_.modify_to_xfwm4_breeze_theme();

        // 5c. 设置 VNC 密码（每次必须设定；Bash: 密码文件不存在时才弹 whiptail）
        vnc_manager_.configure_vnc_password();

        // 5d. VNC 端口：Bash 仅在交互模式弹 whiptail 二选一，auto 默认 5902
        if (interactive) {
            auto port_r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                .add_arg("--title").add_arg(_("gui.vnc.port"))
                .add_arg("--yes-button").add_arg("5902")
                .add_arg("--no-button").add_arg("5903")
                .add_arg("--yesno")
                .add_arg(_("gui.vnc.port_select_prompt"))
                .add_arg("0").add_arg("50")
                .build_string());
            vnc_manager_.config().display = (port_r.exit_code == 0) ? 2 : 3;
        } else {
            vnc_manager_.config().display = 2;
        }
        vnc_manager_.config().update_port();

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

        // 5f. 预建桌面环境需要的用户目录
        if (home != "/root") {
            Executor::shell("mkdir -p " + home + "/.local/share "
                            + home + "/.config/autostart "
                            + home + "/.cache/sessions 2>/dev/null || true");
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

            // Bash 5579-5590: 修复家目录权限 — VNC 配置可能以 root 写入，需 chown 回当前用户
            auto user_name = Executor::shell("id -un 2>/dev/null");
            auto user_group = Executor::shell("id -gn 2>/dev/null");
            std::string uname = user_name.ok() ? user_name.stdout_data : "";
            std::string ugroup = user_group.ok() ? user_group.stdout_data : "";
            trim_newline(uname);
            trim_newline(ugroup);

            if (!uname.empty() && uname != "root") {
                Logger::info(_("gui.vnc.fixing_nonroot_perms"));
                for (const char* dir : {".ICEauthority", ".Xauthority", ".vnc", ".cache", ".dbus", ".local", ".config"}) {
                    std::string p = home + "/" + dir;
                    if (fs::exists(p)) {
                        Executor::passthrough(CommandBuilder("sudo")
                            .add_arg("-E").add_arg("chown").add_arg("-R")
                            .add_arg(uname + ":" + ugroup)
                            .add_arg(p)
                            .build_string() + " 2>/dev/null || true");
                    }
                }
            }
        }

        // 7. WSL 环境网络处理
        if (cfg_.is_wsl) {
            Logger::info(_("gui.wsl_win10_xserver"));
            Logger::info(_("gui.wsl_firewall_hint"));
            Logger::info(_("gui.wsl_hidpi_hint"));

            auto wsl_ver = Executor::shell("uname -r | cut -d '-' -f 2 2>/dev/null");
            std::string ver = wsl_ver.ok() ? wsl_ver.stdout_data : "";
            trim_newline(ver);
            if (ver == "microsoft") {
                auto wsl_r = Executor::shell("ip route list table 0 2>/dev/null | head -1 | "
                    "awk -F 'default via ' '{print $2}' | awk '{print $1}'");
                std::string wsl_ip = wsl_r.ok() ? wsl_r.stdout_data : "";
                trim_newline(wsl_ip);
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

        // ── 三：x11vnc
        Logger::info(std::string(_("gui.section_three")));
        remote_desktop_manager_.x11vnc_warning();
        if (!desktop_manager_.is_auto_install_mode()) {
            if (!Logger::confirm_yes_default(_("gui.confirm_x11vnc")))
                Logger::info(_("gui.x11vnc_skipped"));
            else
                remote_desktop_manager_.configure_x11vnc_remote_desktop_session();
        } else {
            remote_desktop_manager_.configure_x11vnc_remote_desktop_session();
        }
        Logger::info("------------------------");

        // ── 四：noVNC — Bash 原版始终安装 ──
        Logger::info(std::string(_("gui.section_four")) + "：");
        Logger::info(_("gui.vnc.unlock_achievement_hint"));

        remote_desktop_manager_.do_you_want_to_configure_novnc();
    }

    bool GUIManager::configure_xsdl() {
        Logger::step(_("gui.xsdl.configure_step"));

        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            std::string(_("gui.xsdl_config_title")),
            std::string(_("gui.xsdl_config_prompt")),
            "xsdl_display");

        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.xsdl_display_local")), "1",
            [this](MenuContext&) -> bool {
                Logger::ok(std::string(_("gui.xsdl.display_set")) + "127.0.0.1:0");
                return true;
            }));

        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.xsdl_display_custom")), "2",
            [this](MenuContext&) -> bool {
                std::string input_cmd = CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.xsdl_custom_title")))
                        .add_arg("--inputbox").add_arg(std::string(_("gui.xsdl_custom_input")))
                        .add_arg("10").add_arg("40").add_arg("127.0.0.1:0")
                        .build_string();
                std::string display = Executor::tui_select(input_cmd);
                Logger::ok(std::string(_("gui.xsdl.display_set")) + display);
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
        return true;
    }

    bool GUIManager::start_xsdl() {
        Logger::step(_("gui.xsdl.start_step"));

        if (cfg_.is_wsl) {
            vnc_manager_.detect_wsl_environment();
        }

        // G119/G168: WSL — 启动 Windows VcXsrv X 服务器
        if (cfg_.is_wsl) {
            std::string vcxsrv_dir = "/mnt/c/Users/Public/Downloads/VcXsrv";
            if (fs::exists(vcxsrv_dir + "/vcxsrv.exe")) {
                Logger::info(_("gui.xsdl.wsl_launch_vcxsrv"));
                // 先杀掉旧 VcXsrv 进程，再启动新的
                Executor::shell("/mnt/c/WINDOWS/system32/taskkill.exe /f /im vcxsrv.exe "
                                "2>/dev/null; true");
                Executor::shell("cd \"" + vcxsrv_dir + "\" && "
                                "/mnt/c/WINDOWS/system32/cmd.exe /c "
                                "\"start .\\vcxsrv.exe :0 -multiwindow -clipboard -wgl -ac\" "
                                "&>/dev/null &");
            }
        }

        // G118: Android — am start XServer XSDL
        if (cfg_.is_termux) {
            Logger::info(_("gui.xsdl.android_launch"));
            Executor::shell("am start -n x.org.server/x.org.server.MainActivity "
                            "2>/dev/null || true");
            Logger::info(_("gui.xsdl.android_wait"));
            Executor::shell("sleep 6");
        }

        std::string display = cfg_.is_wsl ? (vnc_manager_.config().windows_ip + ":0") : "127.0.0.1:0";
        std::string env = "export DISPLAY=" + display + " "
                          "export PULSE_SERVER=tcp:" + display.substr(0, display.find(':')) + ":" +
                          std::to_string(vnc_manager_.config().pulse_port);

        if (cfg_.is_wsl) {
            Logger::info(_("gui.xsdl.wsl_detect_vcxsrv"));
            Logger::info("DISPLAY=" + display);
        }

        // Bash 原版使用系统 Xsession 脚本，支持任意已安装的桌面环境
        std::string xsession = env + " /etc/X11/xinit/Xsession 2>/dev/null &";
        Executor::passthrough(xsession);

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

    void GUIManager::run_vnc_config_menu() {
        using namespace tmoe::ui;

        // ── Resolution submenu ──
        auto res_menu = make_plugin_menu(
            std::string(_("gui.resolution_title")),
            std::string(_("gui.resolution_prompt")),
            "vnc_resolution");
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_1440x720")), "1",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().resolution_w = 1440;
                vnc_manager_.config().resolution_h = 720;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_1280x720")), "2",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().resolution_w = 1280;
                vnc_manager_.config().resolution_h = 720;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_1920x1080")), "3",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().resolution_w = 1920;
                vnc_manager_.config().resolution_h = 1080;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_2560x1440")), "4",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().resolution_w = 2560;
                vnc_manager_.config().resolution_h = 1440;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_1024x768")), "5",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().resolution_w = 1024;
                vnc_manager_.config().resolution_h = 768;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        res_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.resolution_custom")), "6",
            [this](MenuContext&) -> bool {
                std::string res = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.resolution_title")))
                        .add_arg("--inputbox").add_arg(std::string(_("gui.resolution_prompt")))
                        .add_arg("10").add_arg("50")
                        .build_string());
                if (!res.empty()) {
                    auto xpos = res.find('x');
                    if (xpos != std::string::npos) {
                        vnc_manager_.config().resolution_w = std::stoi(res.substr(0, xpos));
                        vnc_manager_.config().resolution_h = std::stoi(res.substr(xpos + 1));
                        vnc_manager_.configure_vnc_defaults();
                    }
                }
                return true;
            }));
        add_navigation_items(res_menu);

        // ── Depth submenu ──
        auto depth_menu = make_plugin_menu(
            std::string(_("gui.depth_title")),
            std::string(_("gui.depth_prompt")),
            "vnc_depth");
        depth_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.depth_24")), "1",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().pixel_depth = 24;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        depth_menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.depth_16")), "2",
            [this](MenuContext&) -> bool {
                vnc_manager_.config().pixel_depth = 16;
                vnc_manager_.configure_vnc_defaults();
                return true;
            }));
        add_navigation_items(depth_menu);

        // ── Zlib submenu ──
        auto zlib_menu = make_plugin_menu(
            std::string(_("gui.zlib_title")),
            std::string(_("gui.zlib_prompt")),
            "vnc_zlib");
        for (int i = 0; i <= 9; ++i) {
            std::string tag = std::to_string(i + 1);
            std::string label;
            if (i == 0) label = "0 (" + std::string(_("gui.zlib_default")) + ")";
            else if (i == 9) label = "9 (" + std::string(_("gui.zlib_highest")) + ")";
            else label = std::to_string(i);
            zlib_menu->add_child(std::make_shared<LambdaAction>(
                label, tag,
                [this, i](MenuContext&) -> bool {
                    vnc_manager_.config().zlib_level = i;
                    vnc_manager_.configure_vnc_defaults();
                    return true;
                }));
        }
        add_navigation_items(zlib_menu);

        // ── Main VNC config menu ──
        auto menu = make_plugin_menu(
            std::string(_("gui.vnc_config_title")),
            std::string(_("gui.vnc_config_prompt")),
            "vnc_config");

        menu->add_child(LambdaAction::make(
            _("gui.vnc.edit_startvnc"), "1",
            [] {
                Executor::passthrough(
                    "${EDITOR:-nano} /usr/local/bin/startvnc 2>/dev/null || nano /usr/local/bin/startvnc");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.vnc_password")), "2",
            [this] {
                vnc_manager_.configure_vnc_password();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.vnc_switch_server")), "3",
            [this] {
                vnc_manager_.choose_vnc_server();
                vnc_manager_.configure_vnc_defaults();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.vnc_pulseaudio")), "4",
            [this] {
                std::string pa_addr = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.vnc.pulse_server_title"))
                        .add_arg("--inputbox")
                        .add_arg(_("gui.vnc.pulse_server_prompt"))
                        .add_arg("20").add_arg("50")
                        .build_string());
                if (!pa_addr.empty()) {
                    std::string pa_script = "/usr/local/bin/startvnc";
                    auto pa_content = SystemHelper::read_file(fs::path(pa_script));
                    if (contains(pa_content, "PULSE_SERVER")) {
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
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.vnc.edit_xsession"), "5",
            [] {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/X11/xinit/Xsession");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.vnc.edit_tigervnc_config"), "6",
            [this] {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/tigervnc/vncserver-config-tmoe 2>/dev/null || "
                    + editor + " " + vnc_manager_.config().vnc_home_dir.string() + "/config");
                Logger::press_enter();
            }));
        // Zlib submenu entry
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.vnc_zlib")), "7",
            [zlib_menu](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(zlib_menu);
                return true;
            }));
        // Depth submenu entry
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.vnc_depth")), "8",
            [depth_menu](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(depth_menu);
                return true;
            }));
        // Window scaling (inputbox)
        menu->add_child(LambdaAction::make(
            _("gui.vnc.window_scaling"), "9",
            [this] {
                std::string scale = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.vnc.scaling_title"))
                        .add_arg("--inputbox")
                        .add_arg(_("gui.vnc.scaling_prompt"))
                        .add_arg("12").add_arg("50").add_arg("1")
                        .build_string());
                if (!scale.empty()) {
                    int s = 1;
                    try { s = std::stoi(scale); } catch (...) {}
                    Executor::shell(CommandBuilder("dbus-launch")
                                    .add_arg("xfconf-query")
                                    .add_arg("-c").add_arg("xsettings")
                                    .add_arg("-t").add_arg("int")
                                    .add_arg("-np").add_arg("/Gdk/WindowScalingFactor")
                                    .add_arg("-s").add_arg(std::to_string(s))
                                    .build_string() + " 2>/dev/null || true");
                    if (s > 1) {
                        auto os_rel_content = SystemHelper::read_file("/etc/os-release");
                        if (contains(os_rel_content, "Focal Fossa") ||
                            contains(os_rel_content, "focal")) {
                            Executor::shell(CommandBuilder("dbus-launch")
                                .add_arg("xfconf-query").add_arg("-c").add_arg("xfwm4")
                                .add_arg("-t").add_arg("string").add_arg("-np").add_arg("/general/theme")
                                .add_arg("-s").add_arg("Kali-Light-xHiDPI")
                                .build_string() + " 2>/dev/null || true");
                        } else {
                            Executor::shell(CommandBuilder("dbus-launch")
                                .add_arg("xfconf-query").add_arg("-c").add_arg("xfwm4")
                                .add_arg("-t").add_arg("string").add_arg("-np").add_arg("/general/theme")
                                .add_arg("-s").add_arg("Default-xhdpi")
                                .build_string() + " 2>/dev/null || true");
                        }
                    }
                    Logger::ok(std::string(_("gui.vnc.scaling_set")) + scale);
                }
                Logger::press_enter();
            }));
        // Port (inputbox)
        menu->add_child(LambdaAction::make(
            std::string(_("gui.vnc_port")), "10",
            [this] {
                std::string port = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(std::string(_("gui.port_title")))
                        .add_arg("--inputbox").add_arg(std::string(_("gui.port_prompt")))
                        .add_arg("10").add_arg("50")
                        .add_arg(std::to_string(vnc_manager_.config().rfb_port))
                        .build_string());
                if (!port.empty()) {
                    try {
                        int p = std::stoi(port);
                        vnc_manager_.config().display = p - 5900;
                        vnc_manager_.config().rfb_port = p;
                    } catch (...) {}
                    vnc_manager_.configure_vnc_defaults();
                }
                Logger::press_enter();
            }));
        // WSL pulseaudio (editor)
        menu->add_child(LambdaAction::make(
            _("gui.vnc.wsl_pulseaudio"), "11",
            [] {
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /usr/local/etc/tmoe-linux/wsl_pulse_audio");
                Logger::press_enter();
            }));
        // Resolution submenu entry
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.vnc_resolution")), "12",
            [res_menu](MenuContext& ctx) -> bool {
                MenuEngine(ctx).run(res_menu);
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }

    void GUIManager::run_xsdl_config_menu() {
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            std::string(_("gui.xsdl.config_menu_title")),
            std::string(_("gui.xsdl.config_menu_prompt")),
            "xsdl_config");

        const std::string script = "/usr/local/bin/startxsdl";

        menu->add_child(LambdaAction::make(
            _("gui.xsdl.pulse_port"), "1",
            [this, script] {
                std::string val = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.pulse_port_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.pulse_port_prompt"))
                        .add_arg("10").add_arg("50")
                        .build_string());
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@PULSE_SERVER=.*@PULSE_SERVER=\"tcp:localhost:" + val + "\"@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.xsdl.display_number"), "2",
            [this, script] {
                std::string val = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.display_num_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.display_num_prompt"))
                        .add_arg("10").add_arg("50").add_arg("0")
                        .build_string());
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@\\${1:-[^}]*@${1:-127.0.0.1:" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.xsdl.ip_address"), "3",
            [this, script] {
                std::string val = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.ip_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.ip_prompt"))
                        .add_arg("10").add_arg("50").add_arg("127.0.0.1")
                        .build_string());
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@\\${1:-[^:]*@${1:-" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.xsdl.edit_manual"), "4",
            [script] {
                Executor::passthrough("${EDITOR:-nano} " + script + " 2>/dev/null || nano " + script);
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.xsdl.display_switch"), "5",
            [this, script] {
                auto r = Executor::passthrough(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--yesno").add_arg(_("gui.xsdl.display_switch_prompt"))
                        .add_arg("0").add_arg("0")
                        .build_string());
                if (r.exit_code == 0)
                    Executor::shell(
                        "if grep -q '^export.*DISPLAY' " + script + "; then " +
                        CommandBuilder("sed").add_arg("-i").add_arg("/export DISPLAY=/d").add_arg(script)
                        .build_string() +
                        "; else echo 'export DISPLAY=:0' >> " + script + "; fi 2>/dev/null || true");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            _("gui.xsdl.vcxsrv_port"), "6",
            [this, script] {
                std::string val = Executor::tui_select(
                    CommandBuilder(cfg_.tui_bin)
                        .add_arg("--title").add_arg(_("gui.xsdl.vcxsrv_port_title"))
                        .add_arg("--inputbox").add_arg(_("gui.xsdl.vcxsrv_port_prompt"))
                        .add_arg("10").add_arg("50").add_arg(":0")
                        .build_string());
                if (!val.empty())
                    Executor::shell(CommandBuilder("sed")
                                    .add_arg("-i")
                                    .add_arg("s@\\${1:-[^}]*@${1:-127.0.0.1:" + val + "@")
                                    .add_arg(script)
                                    .build_string() + " 2>/dev/null || true");
                Logger::press_enter();
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
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

        if (contains(d, "lxde")) {
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
            if (contains(os_rel_content, "Focal Fossa")) {
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

        // 委托给 DesktopMenuPlugin — 统一桌面安装入口
        tmoe::ui::MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        tmoe::ui::MenuEngine(ctx).run(
            tmoe::ui::menus::make_menu<tmoe::ui::menus::DesktopMenuPlugin>(this));
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

        Logger::info(_("gui.wsl.download_pulseaudio"));
        Executor::shell("cd " + pub_dl + "/pulseaudio && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/pulseaudio/pulseaudio.tar.xz' -o pulseaudio.tar.xz 2>/dev/null && "
                        "tar -Jxvf pulseaudio.tar.xz 2>/dev/null || true");

        Logger::info(_("gui.wsl.download_vcxsrv"));
        Executor::shell("cd " + pub_dl + "/VcXsrv && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/VcXsrv/VcXsrv.tar.xz' -o VcXsrv.tar.xz 2>/dev/null && "
                        "tar -Jxvf VcXsrv.tar.xz 2>/dev/null || true");

        Logger::info(_("gui.wsl.download_tigervnc"));
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
            "sudo ln -sfv \"$TOME_BIN\" /usr/local/bin/tmoe 2>/dev/null || true");
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
            run_vnc_config_menu();
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
        } else if (flag == "--stop-vnc-no-dbus") {
            vnc_manager_.stop_vnc(-1, {.stop_dbus = false});
            return true;
        } else if (flag == "--stop-vnc-no-x11vnc") {
            vnc_manager_.stop_vnc(-1, {.stop_x11vnc = false});
            return true;
        } else if (flag == "--stop-x11vnc") {
            vnc_manager_.stop_vnc(-1, {.x11_mode = true});
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
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            std::string(_("gui.remote_title")),
            std::string(_("gui.remote_prompt")),
            "remote_desktop");

        menu->add_child(LambdaAction::make(
            std::string(_("gui.remote_tightvnc")), "1",
            [this] {
                if (!fs::exists("/usr/local/bin/startvnc")) {
                    Logger::warn(_("gui.remote.no_startvnc_warn"));
                    auto r = Executor::passthrough(CommandBuilder(cfg_.tui_bin)
                        .add_arg("--yesno")
                        .add_arg(_("gui.remote.no_startvnc_continue"))
                        .add_arg("0").add_arg("0")
                        .build_string());
                    if (r.exit_code != 0) return;
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
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.remote_x11vnc")), "2",
            [this] {
                remote_desktop_manager_.run_x11vnc_config_menu();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.remote_xsdl")), "3",
            [this] {
                run_xsdl_config_menu();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.remote_novnc")), "4",
            [this] {
                remote_desktop_manager_.run_novnc_config_menu();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            std::string(_("gui.remote_xrdp")), "5",
            [this] {
                remote_desktop_manager_.run_xrdp_menu();
                Logger::press_enter();
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }
} // namespace tmoe::domain
