#include "gui.h"
#include "vnc_manager.h"
#include "core/system_helper.h"
#include "gui_config/templates.h"
#include "gui_config/registries.h"
#include "core/i18n.h"
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "package_manager.h"

namespace tmoe::domain {
    // ═══════════════════════════════════════════════════════════════
    // 构造 / 析构
    // ═══════════════════════════════════════════════════════════════

    GUIManager::GUIManager(const TmoeConfig &cfg) : cfg_(cfg), vnc_manager_(cfg), desktop_manager_(cfg, vnc_manager_) {
    }

    void GUIManager::first_configure_vnc(std::string_view desktop) {
        // 对应旧 Bash first_configure_startvnc(): VNC 服务端选择 + 密码 + 端口 + HiDPI
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        // 0.5. 卸载 udisks2 (对应旧 Bash remove_udisk_and_gvfs)
        if (cfg_.is_termux && family == DistroFamily::Debian) {
            auto os_rel = Executor::shell(
                "grep -Eq 'Focal Fossa|focal|bionic|Bionic Beaver|Eoan Ermine|buster|stretch|jessie|Deepin 20|Uos 20' /etc/os-release && echo 'yes'");
            if (os_rel.ok() && os_rel.stdout_data.find("yes") != std::string::npos) {
                Logger::info("检测到您处于proot容器环境下，即将为您卸载udisk2和gvfs");
                Executor::passthrough("apt purge -y --allow-change-held-packages ^udisks2 ^gvfs 2>/dev/null || true");
            }
        }

        // dpkg 修复 (对应旧 Bash first_configure_startvnc start 处 dpkg --configure -a)
        if (Executor::has("apt-get")) {
            Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
        }

        vnc_manager_.configure_startvnc();
        vnc_manager_.configure_startxsdl();
        Executor::shell(
            "chmod a+rx -v /usr/local/bin/startvnc /usr/local/bin/stopvnc /usr/local/bin/startxsdl 2>/dev/null || true");

        // 5a. Debian: 安装 VNC 服务端
        if (family == DistroFamily::Debian) {
            // 安装 fonts-noto-color-emoji (老版中有意重复两次以确保安装)
            Executor::passthrough("apt install -y fonts-noto-color-emoji 2>/dev/null || "
                "apt install -y fonts-noto-color-emoji 2>/dev/null || true");
        }

        // 5b. 选择 VNC 服务端 (Tiger vs Tight)
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

        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"Which vnc server do you prefer\""
                                       " --yes-button 'tiger' --no-button 'tight'"
                                       " --yesno \"" + prompt + "\" 0 50");
        if (r.exit_code == 0) {
            vnc_manager_.config().server = "tiger";
            vnc_manager_.config().server_bin = "tigervnc";
        } else if (r.exit_code == 1) {
            vnc_manager_.config().server = "tight";
            vnc_manager_.config().server_bin = "tightvnc";
        }

        vnc_manager_.modify_to_xfwm4_breeze_theme();

        // 5c. 设置 VNC 密码 (对应旧 Bash set_vnc_passwd: 交互式输入)
        auto passwd_check = Executor::shell("[ -s " + vnc_manager_.config().passwd_file.string() + " ] && echo 'yes'");
        if (!passwd_check.ok() || passwd_check.stdout_data.find("yes") == std::string::npos) {
            vnc_manager_.configure_vnc_password(); // 空参数 = 交互式输入，不用硬编码密码
        }

        // 5d. 选择 VNC 端口 (auto mode: 默认5902，跳过提示)
        if (desktop_manager_.is_auto_install_mode()) {
            vnc_manager_.config().display = 2;
            vnc_manager_.config().update_port();
        } else {
            auto port_r = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"VNC PORT\" --yes-button \"5902\" --no-button \"5903\""
                                                " --yesno \"请选择VNC端口✨\\nPlease choose a vnc port\" 0 50");
            if (port_r.exit_code == 0) {
                vnc_manager_.config().display = 2;
                vnc_manager_.config().update_port();
            } else if (port_r.exit_code == 1) {
                vnc_manager_.config().display = 3;
                vnc_manager_.config().update_port();
            }
        }

        // 5e. HiDPI 检测 (auto mode: 默认1440x720)
        if (desktop_manager_.is_auto_install_mode()) {
            vnc_manager_.config().resolution_w = 1440;
            vnc_manager_.config().resolution_h = 720;
        } else {
            detect_and_configure_hidpi(desktop);
        }

        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";

        // ── 对应旧 Bash lines 5478-5504: Neko ASCII art ──
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

        // ── 对应旧 Bash lines 5507-5569: 完整分辨率检测 ──
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
                    // 旧 Bash: awk -F 'x' '{print $2,$1}' -> 宽<高交换
                    int _swapped_w = (_w < _h) ? _h : _w;
                    int _swapped_h = (_w < _h) ? _w : _h;
                    res = std::to_string(_swapped_w) + "x" + std::to_string(_swapped_h);
                    if (_swapped_w >= 2340) tmo_high_dpi = "true";
                } catch (...) {
                }
            }
        }

        // ── 对应旧 Bash tmoe_gui_dpi_02: 分辨率同步到所有配置文件 ──
        if (!res.empty() && vnc_manager_.config().resolution_w > 0 && vnc_manager_.config().resolution_h > 0) {
            std::string res_str = std::to_string(vnc_manager_.config().resolution_w) + "x" + std::to_string(
                                      vnc_manager_.config().resolution_h);
            Executor::shell(
                "sed -i -E 's@(geometry)=.*@\\1=" + res_str +
                "@' /etc/tigervnc/vncserver-config-tmoe 2>/dev/null || true");
            Executor::shell(
                "sed -i -E 's@^(VNC_RESOLUTION)=.*@\\1=" + res_str + "@' /usr/local/bin/startvnc 2>/dev/null || true");
            Executor::shell(
                "sed -i -E 's@^(TMOE_X11_RESOLUTION)=.*@\\1=" + res_str +
                "@' /usr/local/bin/startx11vnc 2>/dev/null || true");
        }

        // 5f. 权限修复: 非 root 用户 chown .vnc/.Xauthority 等
        if (home != "/root") {
            Executor::passthrough("chown -R $(id -un):$(id -gn) " + home +
                                  "/.vnc " + home + "/.Xauthority " + home + "/.ICEauthority "
                                  + home + "/.cache " + home + "/.dbus " + home + "/.local 2>/dev/null || true");
        }

        // 写回配置
        vnc_manager_.configure_vnc_defaults();

        // 6. 创建 ~/startvnc 符号链接 (旧 Bash line 5576)
        if (!fs::exists(home + "/startvnc"))
            Executor::shell("ln -sf /usr/local/bin/startvnc " + home + "/startvnc 2>/dev/null || true");
        // 复制 .vnc 到 /root (旧 Bash lines 5620-5623)
        if (home != "/root") {
            Executor::shell("cp -rpf " + home + "/.vnc /root/ 2>/dev/null || true");
            Executor::shell("chown -R 0:0 /root/.vnc 2>/dev/null || true");
        }

        // 7. WSL 环境: 自动检测宿主机 IP 并设置 DISPLAY/PULSE_SERVER (旧 Bash lines 5624-5649)
        if (cfg_.is_wsl) {
            // 显示防火墙 + 高DPI图片提示
            Logger::info(_("gui.wsl_win10_xserver"));
            Logger::info(_("gui.wsl_firewall_hint"));
            Logger::info(_("gui.wsl_hidpi_hint"));

            auto wsl_ver = Executor::shell("uname -r | cut -d '-' -f 2 2>/dev/null");
            std::string ver = wsl_ver.ok() ? wsl_ver.stdout_data : "";
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r')) ver.pop_back();
            if (ver == "microsoft") {
                // WSL2
                auto wsl_r = Executor::shell("ip route list table 0 2>/dev/null | head -1 | "
                    "awk -F 'default via ' '{print $2}' | awk '{print $1}'");
                std::string wsl_ip = wsl_r.ok() ? wsl_r.stdout_data : "";
                while (!wsl_ip.empty() && (wsl_ip.back() == '\n' || wsl_ip.back() == '\r')) wsl_ip.pop_back();
                if (!wsl_ip.empty()) {
                    Executor::shell("export PULSE_SERVER=" + wsl_ip + " 2>/dev/null || true");
                    Executor::shell("export DISPLAY=" + wsl_ip + ":0 2>/dev/null || true");
                    Logger::info(std::string(_("gui.wsl_ip_modified")) + wsl_ip);
                }
            } else {
                // WSL1
                Logger::info(_("gui.wsl1_detected"));
                Executor::shell("export DISPLAY=127.0.0.1:0 2>/dev/null || true");
            }
            Logger::info(_("gui.wsl_wslg_hint"));
        }

        // 使用说明 (对应旧 Bash first_configure_startvnc 末尾输出)
        Logger::info("------------------------");
        Logger::info("一：");
        Logger::info("关于音频服务无法自动启动的说明：");
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

        // ── 对应旧 Bash 末尾 三：x11vnc (gui:5698-5704) ──
        Logger::info(std::string(_("gui.section_three")) + "：");
        vnc_manager_.check_xvnc_command();
        // x11vnc_warning 包含确认提示; 对应旧 Bash 在 x11vnc_warning 内部的 do_you_want_to_continue
        x11vnc_warning();
        auto x11vnc_confirm = Executor::passthrough(cfg_.tui_bin +
                                                    " --yesno \"" + std::string(_("gui.confirm_x11vnc")) + "\" 0 0");
        if (x11vnc_confirm.exit_code == 0) {
            configure_x11vnc_remote_desktop_session();
        }
        Logger::info("------------------------");

        // ── 对应旧 Bash 末尾 四：novnc (gui:5705-5714) ──
        Logger::info(std::string(_("gui.section_four")) + "：");
        Logger::info("注：配置完本工具所支持的所有VNC,将解锁成就*^^*");
        do_you_want_to_configure_novnc();
    }

    bool GUIManager::install_novnc() {
        Logger::step(_("gui.novnc.installing"));

        // 检查是否已安装
        if (fs::exists("/usr/share/novnc") || Executor::has("websockify")) {
            Logger::ok(_("gui.novnc.already_installed"));
            return true;
        }

        // 安装依赖
        std::vector<std::string> pkgs = {"novnc", "websockify", "python3-numpy", "python3-websockify"};
        for (const auto &pkg: pkgs) {
            Executor::passthrough(cfg_.install_command + " " + pkg + " 2>/dev/null || true");
        }

        // 如果 apt 源没有 novnc，从 git 安装
        if (!fs::exists("/usr/share/novnc")) {
            Logger::info(_("gui.novnc.cloning"));
            Executor::passthrough("git clone --depth=1 https://github.com/novnc/noVNC.git "
                "/opt/novnc 2>/dev/null || true");
            if (fs::exists("/opt/novnc")) {
                // 创建符号链接
                Executor::shell("ln -sf /opt/novnc /usr/share/novnc 2>/dev/null || true");
            }
        }

        if (fs::exists("/usr/share/novnc") || fs::exists("/opt/novnc")) {
            Logger::ok(_("gui.novnc.install_ok"));
            return true;
        }

        Logger::error(_("gui.novnc.install_failed"));
        return false;
    }

    bool GUIManager::configure_novnc() {
        Logger::step(_("gui.novnc.config"));

        // 选择端口
        std::string port_cmd = cfg_.tui_bin +
                               " --title \"" + std::string(_("gui.novnc_port_title")) +
                               "\" --menu \"" + std::string(_("gui.novnc_port_prompt")) + "\" 0 0 0 "
                               "\"36080\" \"" + std::string(_("gui.novnc_port_36080")) + "\" "
                               "\"36081\" \"" + std::string(_("gui.novnc_port_36081")) + "\" "
                               "\"6080\" \"" + std::string(_("gui.novnc_port_6080")) + "\" "
                               "\"custom\" \"" + std::string(_("gui.novnc_port_custom")) + "\"";

        std::string choice = Executor::tui_select(port_cmd);
        if (choice == "custom") {
            std::string input_cmd = cfg_.tui_bin +
                                    " --title \"" + std::string(_("gui.novnc_custom_port_title")) +
                                    "\" --inputbox \"" + std::string(_("gui.novnc_custom_port_input")) +
                                    "\" 10 40 \"36080\"";
            std::string port_str = Executor::tui_select(input_cmd);
            try { novnc_port_ = std::stoi(port_str); } catch (...) { novnc_port_ = 36080; }
        } else if (!choice.empty()) {
            try { novnc_port_ = std::stoi(choice); } catch (...) { novnc_port_ = 36080; }
        }

        Logger::info(_f("gui.novnc.port_set", std::to_string(novnc_port_)));
        return true;
    }

    bool GUIManager::start_novnc(int port) {
        if (port > 0) novnc_port_ = port;

        Logger::step(_("gui.novnc.start"));

        // 确保 noVNC 已安装
        if (!fs::exists("/usr/share/novnc") && !fs::exists("/opt/novnc")) {
            if (!install_novnc()) return false;
        }

        // 确定 noVNC 目录
        std::string novnc_dir = fs::exists("/usr/share/novnc") ? "/usr/share/novnc" : "/opt/novnc";

        // 确保 VNC 服务先启动
        if (!vnc_manager_.is_vnc_running()) {
            Logger::info(_("gui.novnc.vnc_not_running"));
            if (!vnc_manager_.start_vnc()) return false;
        }

        // 启动 websockify 代理
        std::ostringstream cmd;
        cmd << "websockify --web=" << novnc_dir << " "
                << novnc_port_ << " localhost:" << vnc_manager_.config().rfb_port
                << " > /tmp/tmoe_novnc.log 2>&1 &";

        Executor::passthrough(cmd.str());
        Executor::shell("sleep 2");

        Logger::ok(_f("gui.novnc.url", get_novnc_url()));
        Logger::info(_f("gui.novnc.vnc_backend_port", std::to_string(vnc_manager_.config().rfb_port)));
        return true;
    }

    std::string GUIManager::get_novnc_url() const {
        return "http://localhost:" + std::to_string(novnc_port_) + "/vnc.html";
    }

    // ═══════════════════════════════════════════════════════════════
    // XRDP
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_xrdp() {
        Logger::step(_("gui.xrdp.installing"));

        std::vector<std::string> pkgs = {"xrdp", "xorgxrdp"};
        if (!SystemHelper::install_packages(pkgs, cfg_.install_command)) {
            Logger::error(_("gui.xrdp.install_failed"));
            return false;
        }

        // 修复 xrdp 证书权限: key.pem 默认 640 root:ssl-cert，
        // xrdp 用户必须在 ssl-cert 组中才能读取，否则报 Permission denied
        Executor::shell(
            "if getent group ssl-cert >/dev/null 2>&1; then "
            "  usermod -a -G ssl-cert xrdp 2>/dev/null || true; "
            "fi; "
            // 确保 /etc/xrdp 目录权限正确
            "chown -R root:ssl-cert /etc/xrdp/ 2>/dev/null || true; "
            "chmod 640 /etc/xrdp/key.pem 2>/dev/null || true; "
            "chmod 644 /etc/xrdp/cert.pem 2>/dev/null || true");
        // 旧 Bash: chroot/proot 下 xrdp 需要 aid_inet 组才能联网
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            Executor::shell("usermod -a -G aid_inet xrdp 2>/dev/null || true");
        }

        // 配置 polkit 规则 (允许远程连接)
        std::string polkit_rule =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<!DOCTYPE policyconfig PUBLIC \"-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN\"\n"
                "\"http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd\">\n"
                "<policyconfig>\n"
                "  <action id=\"org.freedesktop.policykit.pkexec.run-xrdp-sesman\">\n"
                "    <description>Run XRDP session manager</description>\n"
                "    <allow_any>yes</allow_any>\n"
                "    <allow_inactive>yes</allow_inactive>\n"
                "    <allow_active>yes</allow_active>\n"
                "  </action>\n"
                "</policyconfig>\n";
        SystemHelper::write_file("/usr/share/polkit-1/actions/xrdp-sesman.policy", polkit_rule);

        // 写入 WSL 兼容的 xrdp.ini (必须先于 startwm.sh，因为后者内部 restart 需要完整 ini)
        set_xrdp_port(3389);

        // 配置 startwm.sh
        configure_xrdp_session("xfce");

        // 启动
        Executor::passthrough("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null || true");

        Logger::ok(_("gui.xrdp.install_ok"));
        return true;
    }

    bool GUIManager::configure_xrdp_session(std::string_view desktop) {
        // 对应旧 Bash xrdp_onekey (5053-5120) + configure_xrdp_remote_desktop_session (4998-5023)
        // 不覆盖发行版自带的 startwm.sh，而是在其基础上修改
        Executor::shell("mkdir -p /etc/xrdp 2>/dev/null");

        // 如果发行版未提供 startwm.sh，生成包含标准环境初始化的模板.
        // 必须保留 /etc/profile 等加载逻辑，否则 PATH/LANG 等变量缺失。
        if (!fs::exists("/etc/xrdp/startwm.sh")) {
            SystemHelper::write_file("/etc/xrdp/startwm.sh",
                                     "#!/bin/sh\n"
                                     "# tmoe-linux xrdp startwm\n\n"
                                     "if test -r /etc/profile; then\n"
                                     "    . /etc/profile\n"
                                     "fi\n\n"
                                     "test -x /etc/X11/Xsession && exec /etc/X11/Xsession\n"
                                     "exec /etc/X11/xinit/Xsession\n");
            Executor::shell("chmod +x /etc/xrdp/startwm.sh 2>/dev/null || true");
        }

        // 注入 WSL/WSLg/GPU 环境修复 (只在尚不存在时插入，避免重复追加)
        // 注意: 这些 sed 按顺序执行，行号插入前面的语句会影响后续插入位置.
        // 用 grep 先检查再插入，保证幂等。
        auto ensure_line = [](const std::string &pattern, const std::string &line, int at_line) {
            std::string cmd =
                    "if ! grep -q '" + pattern + "' /etc/xrdp/startwm.sh 2>/dev/null; then "
                    "  sed -i '" + std::to_string(at_line) + "i\\" + line +
                    "' /etc/xrdp/startwm.sh 2>/dev/null || true; "
                    "fi";
            Executor::shell(cmd);
        };

        ensure_line("unset WAYLAND_DISPLAY",
                    "unset WAYLAND_DISPLAY", 2);
        ensure_line("unset XDG_RUNTIME_DIR",
                    "unset XDG_RUNTIME_DIR", 3);
        ensure_line("LIBGL_ALWAYS_SOFTWARE",
                    "export LIBGL_ALWAYS_SOFTWARE=1", 4);
        ensure_line("GALLIUM_DRIVER",
                    "export GALLIUM_DRIVER=llvmpipe", 5);
        ensure_line("^export PULSE_SERVER",
                    "export PULSE_SERVER=127.0.0.1", 6);

        // xrdp_onekey 风格: 替换默认 Xsession 路径为 xinit 版本
        Executor::shell(
            "sed -i 's@exec /etc/X11/Xsession@exec /etc/X11/xinit/Xsession@g;"
            "s:exec /bin/sh /etc/X11/Xsession:exec /etc/X11/xinit/Xsession:g' "
            "/etc/xrdp/startwm.sh 2>/dev/null || true");

        // 委托给 configure_xrdp_remote_desktop_session 设置具体桌面会话
        // (对应旧 Bash: cd /etc/xrdp; sed /Xsession/d; cat >> startwm; sed dbus-launch)
        // 注意: 需要传会话命令（如 xfce4-session），而非桌面名称（如 xfce）
        DesktopInfo info = desktop_manager_.get_desktop_info(desktop);
        std::string session_cmd = Executor::has(info.session_cmd1)
                                      ? std::string(info.session_cmd1)
                                      : std::string(info.session_cmd2);
        configure_xrdp_remote_desktop_session(session_cmd);
        return true;
    }

    bool GUIManager::start_xrdp() {
        Logger::step(_("gui.xrdp.starting"));
        if (Executor::passthrough("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null").ok()) {
            Logger::ok(_("gui.xrdp.started"));
            return true;
        }
        // 备用: 直接启动 xrdp 守护进程
        if (Executor::passthrough("xrdp 2>/dev/null &").ok()) {
            Logger::ok(_("gui.xrdp.started_direct"));
            return true;
        }
        Logger::error(_("gui.xrdp.start_failed"));
        return false;
    }

    bool GUIManager::stop_xrdp() {
        Logger::step(_("gui.xrdp.stopping"));
        Executor::passthrough("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || "
            "pkill xrdp 2>/dev/null || true");
        Logger::ok(_("gui.xrdp.stopped"));
        return true;
    }

    bool GUIManager::restart_xrdp() {
        stop_xrdp();
        Executor::shell("sleep 1");
        bool ok = start_xrdp();
        if (ok) {
            Logger::info(_("gui.xrdp.connection_info"));
            Logger::info(_("gui.xrdp.wsl_info"));
        }
        return ok;
    }

    bool GUIManager::set_xrdp_port(int port) {
        // xrdp 守护进程以 xrdp 用户运行，必须确保它能读取 /etc/xrdp/
        Executor::shell(
            "mkdir -p /etc/xrdp 2>/dev/null; "
            // 确保目录和文件对 xrdp 用户可读
            "chmod 755 /etc/xrdp 2>/dev/null || true; "
            // 确保 xrdp 用户存在 (proot/容器里 postinst 可能没创建)
            "id xrdp >/dev/null 2>&1 || useradd -r -s /usr/sbin/nologin xrdp 2>/dev/null || true");

        std::string port_str = std::to_string(port);

        // ── 始终写入用户验证通过的完整 xrdp.ini 模板 ──
        // 不再用 sed 修补包默认配置：默认配置在不同发行版/proot 下有差异，
        // 且段落级 port (如 [Xorg] port=-1) 各发行版默认值不尽相同，
        // 直接写完整模板最可靠。
        SystemHelper::write_file("/etc/xrdp/xrdp.ini",
                                 "[Globals]\n"
                                 "address=0.0.0.0\n"
                                 "ini_version=1\n"
                                 "fork=true\n"
                                 "port=tcp://0.0.0.0:" + port_str + "\n"
                                 "use_vsock=false\n"
                                 "tcp_nodelay=true\n"
                                 "tcp_keepalive=true\n"
                                 "security_layer=rdp\n"
                                 "crypt_level=none\n"
                                 "certificate=\n"
                                 "key_file=\n"
                                 "ssl_protocols=TLSv1.2, TLSv1.3\n"
                                 "autorun=\n"
                                 "allow_channels=true\n"
                                 "allow_multimon=true\n"
                                 "bitmap_cache=true\n"
                                 "bitmap_compression=true\n"
                                 "bulk_compression=true\n"
                                 "max_bpp=32\n"
                                 "new_cursors=true\n"
                                 "use_fastpath=both\n"
                                 "\n"
                                 "[Logging]\n"
                                 "LogFile=xrdp.log\n"
                                 "LogLevel=INFO\n"
                                 "EnableSyslog=true\n"
                                 "\n"
                                 "[Channels]\n"
                                 "rdpdr=true\n"
                                 "rdpsnd=true\n"
                                 "drdynvc=true\n"
                                 "cliprdr=true\n"
                                 "rail=true\n"
                                 "xrdpvr=true\n"
                                 "tcutils=true\n"
                                 "\n"
                                 "[Xorg]\n"
                                 "name=Xorg\n"
                                 "lib=libxup.so\n"
                                 "username=ask\n"
                                 "password=ask\n"
                                 "ip=127.0.0.1\n"
                                 "port=-1\n"
                                 "code=20\n"
                                 "\n"
                                 "[Xvnc]\n"
                                 "name=Xvnc\n"
                                 "lib=libvnc.so\n"
                                 "username=ask\n"
                                 "password=ask\n"
                                 "ip=127.0.0.1\n"
                                 "port=-1\n"
                                 "\n"
                                 "[vnc-any]\n"
                                 "name=vnc-any\n"
                                 "lib=libvnc.so\n"
                                 "ip=ask\n"
                                 "port=5900\n"
                                 "username=na\n"
                                 "password=ask\n"
                                 "\n"
                                 "[neutrinordp-any]\n"
                                 "name=neutrinordp-any\n"
                                 "lib=libxrdpneutrinordp.so\n"
                                 "ip=ask\n"
                                 "port=3389\n"
                                 "username=ask\n"
                                 "password=ask\n");
        Executor::shell("chmod 644 /etc/xrdp/xrdp.ini 2>/dev/null || true");

        // 验证 xrdp 用户能否读取配置
        auto readable = Executor::shell(
            "sudo -u xrdp cat /etc/xrdp/xrdp.ini >/dev/null 2>&1 && echo 'ok' || "
            "cat /etc/xrdp/xrdp.ini >/dev/null 2>&1 && echo 'ok_root' || echo 'fail'");
        if (readable.stdout_data.find("ok") == std::string::npos) {
            Logger::warn("xrdp 守护进程可能无法读取配置! 检查权限: ls -la /etc/xrdp/");
            Logger::debug("可读性检测: " + readable.stdout_data);
        }

        Logger::ok(_f("gui.xrdp.port_changed", port_str));
        Logger::info(_("gui.xrdp.restart_needed"));
        return true;
    }

    bool GUIManager::remove_xrdp() {
        Logger::step(_("gui.xrdp.removing"));
        Executor::passthrough(cfg_.remove_command + " xrdp xorgxrdp 2>/dev/null || true");
        Logger::ok("XRDP 已卸载");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // XSDL / VcXsrv / WSL
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::configure_xsdl() {
        Logger::step("配置 XSDL 连接...");

        // 设置 DISPLAY 变量
        std::string display_cmd = cfg_.tui_bin +
                                  " --title \"" + std::string(_("gui.xsdl_config_title")) +
                                  "\" --menu \"" + std::string(_("gui.xsdl_config_prompt")) + "\" 0 0 0 "
                                  "\"1\" \"" + std::string(_("gui.xsdl_display_local")) + "\" "
                                  "\"2\" \"" + std::string(_("gui.xsdl_display_custom")) + "\"";

        std::string choice = Executor::tui_select(display_cmd);
        std::string display = "127.0.0.1:0";

        if (choice == "2") {
            std::string input_cmd = cfg_.tui_bin +
                                    " --title \"" + std::string(_("gui.xsdl_custom_title")) +
                                    "\" --inputbox \"" + std::string(_("gui.xsdl_custom_input")) +
                                    "\" 10 40 \"127.0.0.1:0\"";
            display = Executor::tui_select(input_cmd);
            // tui_select 已去除尾部换行
        }

        // DISPLAY should be set via C++ setenv instead of a standalone subshell export
        Logger::ok("XSDL DISPLAY 设置为: " + display);
        return true;
    }

    bool GUIManager::start_xsdl() {
        Logger::step("启动 XSDL/VcXsrv 模式...");

        if (cfg_.is_wsl) {
            vnc_manager_.detect_wsl_environment();
        }

        // 设置 DISPLAY
        std::string display = cfg_.is_wsl ? (vnc_manager_.config().windows_ip + ":0") : "127.0.0.1:0";
        std::string env = "export DISPLAY=" + display + " "
                          "export PULSE_SERVER=tcp:" + display.substr(0, display.find(':')) + ":" +
                          std::to_string(vnc_manager_.config().pulse_port);

        // WSL 下启动 VcXsrv
        if (cfg_.is_wsl) {
            Logger::info("检测到 WSL 环境，请在 Windows 上启动 VcXsrv");
            Logger::info("DISPLAY=" + display);
        }

        // 尝试启动桌面会话
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

        // 写入 ~/.bashrc 或配置文件
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
        // 对应旧 Bash tools/gui/wslg: 在 WSLg 下启动独立 Xwayland 并启动桌面会话
        Logger::step("启动 WSLg (Xwayland)...");

        if (!cfg_.is_wsl) {
            Logger::warn("WSLg 仅适用于 Win11 WSL2 环境");
            return false;
        }

        // 清理 X 锁文件 (对应旧 Bash remove_xsession_lock)
        Executor::shell("rm -vf /tmp/.X" + std::to_string(display_port) +
                        "-lock /tmp/.X11-unix/X" + std::to_string(display_port) +
                        " 2>/dev/null || true");

        // 停止 VNC (对应旧 Bash AUTO_STOP_VNC)
        Executor::shell("stopvnc -no-stop-dbus 2>/dev/null || true");

        // 启动 D-Bus
        vnc_manager_.launch_dbus_daemon();

        // 启动 Xwayland，关键: unset WAYLAND_DISPLAY 防止递归冲突
        std::string cmd = "unset WAYLAND_DISPLAY; Xwayland :" + std::to_string(display_port) + " -noreset &";
        Executor::passthrough(cmd);
        Executor::shell("sleep 1");

        Logger::ok("WSLg Xwayland 已启动 — DISPLAY=:" + std::to_string(display_port));
        Logger::info("现在可以运行 GUI 程序，画面将显示在 Windows 宿主机上");
        return true;
    }

    bool GUIManager::beautify_desktop() {
        Logger::step("桌面美化...");
        return true; // 由 TUI 菜单驱动
    }

    bool GUIManager::install_theme(std::string_view theme) {
        Logger::step("安装桌面主题: " + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::theme_package_name(name_lower);
        return Executor::passthrough(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok();
    }

    bool GUIManager::install_icon_theme(std::string_view theme) {
        Logger::step("安装图标主题: " + std::string(theme));
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::string pkg_name = gui_config::icon_theme_package_name(name_lower);
        return Executor::passthrough(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok();
    }

    bool GUIManager::set_wallpaper(std::string_view path) {
        Logger::step("设置壁纸...");

        if (!path.empty() && fs::exists(path)) {
            // XFCE 壁纸设置
            std::string cmd = "xfconf-query -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image "
                              "-s " + std::string(path) + " 2>/dev/null &";
            Executor::shell(cmd);
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
        return SystemHelper::install_packages({"plank"}, cfg_.install_command);
    }

    // ═══════════════════════════════════════════════════════════════
    // PulseAudio (保留现有实现，略作优化)
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::start_pulseaudio_bridge() const {
        // WSL/WSL2: PulseAudio 运行在 Windows 宿主机，Linux 内部不启动
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

        // PulseAudio 拒绝以 root 运行；如有原始用户则降权
        std::string prefix;
        if (!cfg_.is_termux && std::getenv("SUDO_USER")) {
            prefix = std::string("su - ") + std::getenv("SUDO_USER") + " -c ";
        }

        // 启用 TCP 模块，仅允许本地匿名访问，禁用自动空闲退出
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
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.main_title")) + "\""
                                   " --menu \"" + std::string(_("gui.main_menu_prompt")) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.install_de")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.remote_title")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.beautify")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back_upper")) + "\"";

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
        // 对应旧 Bash modify_other_vnc_conf (精简为7项核心配置)
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"Modify vnc server conf\""
                                   " --menu \"Type startvnc to start vncserver,输入startvnc启动vnc服务\" 0 0 0 "
                                   "\"edit_startvnc\" \"Edit startvnc 编辑vnc启动脚本\" "
                                   "\"1\" \"" + std::string(_("gui.vnc_password")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.vnc_switch_server")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.vnc_resolution")) + " " + std::to_string(
                                       vnc_manager_.config().resolution_w) + "x" +
                                   std::to_string(vnc_manager_.config().resolution_h) + ")\" "
                                   "\"4\" \"" + std::string(_("gui.vnc_port")) + " " + std::to_string(
                                       vnc_manager_.config().rfb_port) + ")\" "
                                   "\"5\" \"" + std::string(_("gui.vnc_depth")) + " " + std::to_string(
                                       vnc_manager_.config().pixel_depth) + ")\" "
                                   "\"6\" \"" + std::string(_("gui.vnc_zlib")) + " " + std::to_string(
                                       vnc_manager_.config().zlib_level) + ")\" "
                                   "\"7\" \"" + std::string(_("gui.vnc_pulseaudio")) + "\" "
                                   "\"8\" \"Edit xsession\" "
                                   "\"9\" \"Edit tigervnc-config\" "
                                   "\"10\" \"Window scaling factor\" "
                                   "\"11\" \"WSL pulseaudio(only for windows)\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

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
                std::string res_cmd = cfg_.tui_bin +
                                      " --title \"" + std::string(_("gui.resolution_title")) +
                                      "\" --menu \"" + std::string(_("gui.resolution_prompt")) + "\" 0 0 0 "
                                      "\"1440x720\"  \"" + std::string(_("gui.resolution_1440x720")) + "\" "
                                      "\"1280x720\"  \"" + std::string(_("gui.resolution_1280x720")) + "\" "
                                      "\"1920x1080\" \"" + std::string(_("gui.resolution_1920x1080")) + "\" "
                                      "\"2560x1440\" \"" + std::string(_("gui.resolution_2560x1440")) + "\" "
                                      "\"1024x768\"  \"" + std::string(_("gui.resolution_1024x768")) + "\" "
                                      "\"custom\"    \"" + std::string(_("gui.resolution_custom")) + "\"";
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
                std::string port_cmd = cfg_.tui_bin +
                                       " --title \"" + std::string(_("gui.port_title")) +
                                       "\" --menu \"" + std::string(_("gui.port_prompt")) + "\" 0 0 0 "
                                       "\"5902\" \"" + std::string(_("gui.port_5902")) + "\" "
                                       "\"5903\" \"" + std::string(_("gui.port_5903")) + "\" "
                                       "\"5901\" \"" + std::string(_("gui.port_5901")) + "\"";
                std::string port = Executor::tui_select(port_cmd);
                if (!port.empty()) {
                    int p = std::stoi(port);
                    vnc_manager_.config().display = p - 5900;
                    vnc_manager_.config().rfb_port = p;
                    Executor::shell(
                        "sed -i 's@tmoe-linux.*:.*@tmoe-linux :" + std::to_string(vnc_manager_.config().display) +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
                    Executor::shell(
                        "sed -i 's@VNC_DISPLAY=.*@VNC_DISPLAY=" + std::to_string(vnc_manager_.config().display) +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
                }
            } else if (choice == "5") {
                std::string depth_cmd = cfg_.tui_bin +
                                        " --title \"" + std::string(_("gui.depth_title")) +
                                        "\" --menu \"" + std::string(_("gui.depth_prompt")) + "\" 0 0 0 "
                                        "\"24\" \"" + std::string(_("gui.depth_24")) + "\" "
                                        "\"16\" \"" + std::string(_("gui.depth_16")) + "\"";
                std::string depth = Executor::tui_select(depth_cmd);
                if (!depth.empty()) vnc_manager_.config().pixel_depth = std::stoi(depth);
            } else if (choice == "6") {
                std::string zlib_cmd = cfg_.tui_bin +
                                       " --title \"" + std::string(_("gui.zlib_title")) +
                                       "\" --menu \"" + std::string(_("gui.zlib_prompt")) + "\" 0 0 0 "
                                       "\"0\" \"" + std::string(_("gui.zlib_0")) + "\" "
                                       "\"3\" \"" + std::string(_("gui.zlib_3")) + "\" "
                                       "\"6\" \"" + std::string(_("gui.zlib_6")) + "\" "
                                       "\"9\" \"" + std::string(_("gui.zlib_9")) + "\"";
                std::string zlib = Executor::tui_select(zlib_cmd);
                if (!zlib.empty()) vnc_manager_.config().zlib_level = std::stoi(zlib);
            } else if (choice == "7") {
                std::string pa_cmd = cfg_.tui_bin +
                                     " --title \"MODIFY PULSE SERVER ADDRESS\""
                                     " --inputbox \"输入 PulseAudio 服务器地址, linux 默认为 127.0.0.1, WSL2 为宿主机 ip\\\\n"
                                     "Android-Termux 需输 pulseaudio --start\\\\n"
                                     "例如 192.168.1.3:4713\" 20 50";
                std::string pa_addr = Executor::tui_select(pa_cmd);
                if (!pa_addr.empty()) {
                    std::string pa_script = "/usr/local/bin/startvnc";
                    auto pa_check = Executor::shell(
                        "grep -q '^export.*PULSE_SERVER' " + pa_script + " 2>/dev/null && echo 'yes'");
                    if (pa_check.ok() && pa_check.stdout_data.find("yes") != std::string::npos) {
                        Executor::shell(
                            "sed -i 's@^export PULSE_SERVER=.*@export PULSE_SERVER=" + pa_addr + "@' " + pa_script +
                            " 2>/dev/null || true");
                    } else {
                        Executor::shell(
                            "sed -i '4 a\\export PULSE_SERVER=" + pa_addr + "' " + pa_script + " 2>/dev/null || true");
                    }
                    Logger::ok("PULSE_SERVER 已更新为: " + pa_addr);
                }
            } else if (choice == "8") {
                // Edit xsession
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/X11/xinit/Xsession");
            } else if (choice == "9") {
                // Edit tigervnc-config
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /etc/tigervnc/vncserver-config-tmoe");
            } else if (choice == "10") {
                // Window scaling factor
                std::string scale_cmd = cfg_.tui_bin +
                                        " --title \"Window scaling factor\""
                                        " --inputbox \"请选择缩放因子 (1 或 2)\\n1 = 正常, 2 = 2倍缩放\\n"
                                        "1 = normal, 2 = HiDPI\" 12 50 \"1\"";
                std::string scale = Executor::tui_select(scale_cmd);
                if (!scale.empty() && (scale == "1" || scale == "2")) {
                    Executor::shell("dbus-launch xfconf-query -c xsettings -t int -np /Gdk/WindowScalingFactor -s " +
                                    scale + " 2>/dev/null || true");
                    // Focal Fossa special case: adjust theme for HiDPI (only when scale > 1)
                    if (scale == "2") {
                        auto os_rel = Executor::shell(
                            "grep -q 'Focal Fossa\\|focal' /etc/os-release && echo 'yes'");
                        if (os_rel.ok() && os_rel.stdout_data.find("yes") != std::string::npos) {
                            Executor::shell(
                                "dbus-launch xfconf-query -c xfwm4 -t string -np /general/theme -s \"Kali-Light-xHiDPI\""
                                " 2>/dev/null || true");
                            Logger::info("Focal Fossa: 已设置主题为 Kali-Light-xHiDPI");
                        }
                    }
                    Logger::ok("WindowScalingFactor 已设置为 " + scale);
                }
            } else if (choice == "11") {
                // WSL pulseaudio (only for windows)
                std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
                Executor::passthrough(editor + " /usr/local/etc/tmoe-linux/wsl_pulse_audio");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_desktop_install_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + _("gui.de_install_title") + "\""
                                   " --menu \"" + "" + "\" 0 0 0 "
                                   "\"1\" \"" + _("gui.de_rootless") + "\" "
                                   "\"2\" \"" + _("gui.de_rootful") + "\" "
                                   "\"3\" \"" + _("gui.de_install_wm") + "\" "
                                   "\"4\" \"" + _("gui.de_install_dm") + "\" "
                                   "\"0\" \"" + _("menu.tui.back") + "\"";

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
        // 对应旧 Bash tmoe_container_desktop (循环)
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + _("gui.de_rootless") + "\""
                                   " --menu \"\" 0 0 0 ";
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root) {
                    menu_cmd += "\"" + std::to_string(idx++) + "\" \"" + d.name + " (" + d.compat_notes + ")\" ";
                }
            }
            menu_cmd += "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu_cmd);
            if (ch == "0" || ch.empty()) break;
            int sel = std::stoi(ch);
            int i = 0;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (!d.requires_root) {
                    ++i;
                    if (i == sel) {
                        desktop_manager_.install_desktop(d.id);
                        // 对应旧 Bash: 装完后提示可继续装更多桌面或去配置远程桌面
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
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + _("gui.de_rootful") + "\""
                                   " --menu \"\" 0 0 0 ";
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.requires_root) {
                    menu_cmd += "\"" + std::to_string(idx++) + "\" \"" + d.name + " (" + d.compat_notes + ")\" ";
                }
            }
            menu_cmd += "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu_cmd);
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
        // 对应旧 Bash window_manager_installation (gui:768-1085)
        // 菜单包含所有 WM 名称+描述，匹配旧 Bash 的 50 项 WM 菜单
        while (true) {
            std::string wm_menu = cfg_.tui_bin +
                                  " --title \"" + std::string(_("gui.wm_title")) +
                                  "\" --menu \"" + std::string(_("gui.wm_prompt")) + "\" 0 0 0 ";
            int idx = 1;
            for (const auto &d: desktop_manager_.desktop_registry()) {
                if (d.is_window_manager) {
                    wm_menu += "\"" + std::to_string(idx++) + "\" \""
                            + d.name + (d.compat_notes.empty() ? "" : " (" + d.compat_notes + ")") + "\" ";
                }
            }
            wm_menu += "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
            auto ch = Executor::tui_select(wm_menu);
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
        std::string dm_menu = cfg_.tui_bin +
                              " --title \"" + std::string(_("gui.dm_title")) +
                              "\" --menu \"" + std::string(_("gui.dm_prompt")) + "\" 0 0 0 "
                              "\"1\" \"lightdm: 支持跨桌面,可使用各种前端\" "
                              "\"2\" \"sddm: 现代化DM,替代KDE4的KDM\" "
                              "\"3\" \"gdm: GNOME默认DM\" "
                              "\"4\" \"slim: Lightweight轻量\" "
                              "\"5\" \"lxdm: LXDE默认DM(独立于桌面环境)\" "
                              "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
        auto ch = Executor::tui_select(dm_menu);
        if (ch == "1") {
            SystemHelper::install_packages({"lightdm", "lightdm-gtk-greeter"}, cfg_.install_command);
            desktop_manager_.tmoe_display_manager_systemctl("lightdm", "lightdm");
        } else if (ch == "2") {
            SystemHelper::install_packages({"sddm", "sddm-theme-breeze"}, cfg_.install_command);
            desktop_manager_.tmoe_display_manager_systemctl("sddm", "sddm");
        } else if (ch == "3") {
            SystemHelper::install_packages({"gdm3"}, cfg_.install_command);
            desktop_manager_.tmoe_display_manager_systemctl("gdm3", "gdm");
        } else if (ch == "4") {
            SystemHelper::install_packages({"slim"}, cfg_.install_command);
            desktop_manager_.tmoe_display_manager_systemctl("slim", "slim");
        } else if (ch == "5") {
            SystemHelper::install_packages({"lxdm"}, cfg_.install_command);
            desktop_manager_.tmoe_display_manager_systemctl("lxdm", "lxdm");
        }
    }

    void GUIManager::run_remote_desktop_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.remote_title")) +
                                   "\" --menu \"" + std::string(_("gui.remote_prompt")) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.remote_tightvnc")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.remote_x11vnc")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.remote_xsdl")) + "\" "
                                   "\"4\" \"" + std::string(_("gui.remote_novnc")) + "\" "
                                   "\"5\" \"" + std::string(_("gui.remote_xrdp")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                // 对应旧 Bash modify_vnc_conf: 先 yesno "分辨率/其它" 再进入子菜单
                if (!fs::exists("/usr/local/bin/startvnc")) {
                    Logger::warn("未检测到 startvnc，您可能尚未安装图形桌面");
                    auto r = Executor::passthrough(cfg_.tui_bin +
                                                   " --yesno \"未检测到 startvnc，是否继续编辑配置？\" 0 0");
                    if (r.exit_code != 0) continue;
                }
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"modify vnc configuration\""
                                               " --yes-button \"分辨率 resolution\" --no-button \"其它 other\""
                                               " --yesno \"Which configuration do you want to modify?\" 9 50");
                if (r.exit_code == 0) {
                    // 分辨率 → inputbox
                    std::string cmd = cfg_.tui_bin +
                                      " --title \"请输入分辨率\""
                                      " --inputbox \"例如 1920x1080, 1440x720, 1280x1024\" 15 50 \"" +
                                      std::to_string(vnc_manager_.config().resolution_w) + "x" +
                                      std::to_string(vnc_manager_.config().resolution_h) + "\"";
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
                run_x11vnc_config_menu();
            } else if (choice == "3") {
                run_xsdl_config_menu();
            } else if (choice == "4") {
                run_novnc_config_menu();
            } else if (choice == "5") {
                run_xrdp_menu();
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_x11vnc_config_menu() {
        // 对应旧 Bash configure_x11vnc (8项)
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"CONFIGURE x11vnc\""
                               " --menu \"Type startx11vnc to start vncserver\" 0 0 0 "
                               "\"1\" \"pulse_server 音频服务\" "
                               "\"2\" \"resolution 分辨率\" "
                               "\"3\" \"port 端口\" "
                               "\"4\" \"修改 startx11vnc 启动脚本\" "
                               "\"5\" \"remove 卸载/移除\" "
                               "\"6\" \"readme 进程管理说明\" "
                               "\"7\" \"password 密码\" "
                               "\"8\" \"read doc 阅读文档\" "
                               "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") vnc_manager_.modify_x11vnc_pulse_server();
            else if (ch == "2") vnc_manager_.modify_x11vnc_resolution();
            else if (ch == "3") vnc_manager_.modify_x11vnc_port();
            else if (ch == "4")
                Executor::passthrough(
                    "${EDITOR:-nano} /usr/local/bin/startx11vnc 2>/dev/null || nano /usr/local/bin/startx11vnc");
            else if (ch == "5") {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 x11vnc？\" 0 0");
                if (r.exit_code == 0) {
                    for (const auto &pkg: {"x11vnc", "x11vnc-data"})
                        Executor::passthrough(cfg_.remove_command + " " + pkg + " 2>/dev/null || true");
                }
            } else if (ch == "6") {
                Logger::info("x11vnc 进程管理:");
                Logger::info("  启动: startx11vnc");
                Logger::info("  停止: stopvnc (会同时停止 tightvnc)");
                Logger::info("  查看: ps aux | grep x11vnc");
            } else if (ch == "7") Executor::passthrough("x11vncpasswd 2>/dev/null || x11vnc -storepasswd");
            else if (ch == "8") {
                Logger::info("x11vnc 文档:");
                Logger::info("  startx11vnc — 启动 x11vnc (自动启动 Xvfb)");
                Logger::info("  stopvnc — 停止所有 VNC 服务 (包括 x11vnc)");
                Logger::info("  x11vnc 连接到真实 X 桌面: x11vnc -display :0");
                Logger::info("  x11vnc 配合 Xvfb: x11vnc -display :233");
                Logger::info("  更多: man x11vnc");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_novnc_config_menu() {
        // 对应旧 Bash modify_novnc_conf → configure_novnc (3项)
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"CONFIGURE NOVNC\""
                               " --menu \"Type novnc to start novnc.输novnc启动novnc\" 0 0 0 "
                               "\"1\" \"port 端口\" "
                               "\"2\" \"修改 startnovnc 启动脚本\" "
                               "\"3\" \"remove 卸载/移除\" "
                               "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") {
                std::string port_cmd = cfg_.tui_bin +
                                       " --title \"请输入端口\""
                                       " --inputbox \"Please type the novnc port, the default is 36080\" 10 50 \"36080\"";
                std::string port = Executor::tui_select(port_cmd);
                if (!port.empty())
                    Executor::shell(
                        "sed -i 's@NOVNC_PORT=.*@NOVNC_PORT=" + port + "@' /usr/local/bin/novnc 2>/dev/null || true");
            } else if (ch == "2") {
                Executor::passthrough("${EDITOR:-nano} /usr/local/bin/novnc 2>/dev/null || nano /usr/local/bin/novnc");
            } else if (ch == "3") {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 noVNC？\" 0 0");
                if (r.exit_code == 0) {
                    Executor::passthrough("pip3 uninstall -y numpy websockify 2>/dev/null || "
                        "sudo -H pip3 uninstall -y numpy websockify 2>/dev/null || true");
                    Executor::passthrough("rm -rfv /usr/local/bin/novnc ${TMOE_LINUX_DIR}/novnc 2>/dev/null || true");
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_xsdl_config_menu() {
        // 对应旧 Bash modify_xsdl_conf (6项)
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"Modify x server conf\""
                               " --menu \"Type startxsdl to start x11.输startxsdl启动x11\" 0 50 0 "
                               "\"1\" \"Pulse server port 音频端口\" "
                               "\"2\" \"Display number 显示编号\" "
                               "\"3\" \"ip address\" "
                               "\"4\" \"Edit manually 手动编辑\" "
                               "\"5\" \"DISPLAY switch 转发显示开关(仅qemu)\" "
                               "\"6\" \"VcXsrv显示端口(仅win10)\" "
                               "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            std::string script = "/usr/local/bin/startxsdl";
            if (ch == "1") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"MODIFY PULSE SERVER PORT\""
                                  " --inputbox \"Please type the pulse server port\" 10 50";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(
                        "sed -i 's@PULSE_SERVER=.*@PULSE_SERVER=\"tcp:localhost:" + val + "\"@' " + script +
                        " 2>/dev/null || true");
            } else if (ch == "2") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"DISPLAY NUMBER\""
                                  " --inputbox \"请输入显示编号 (默认0)\" 10 50 \"0\"";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(
                        "sed -i 's@DISPLAY_NUM=.*@DISPLAY_NUM=" + val + "@' " + script + " 2>/dev/null || true");
            } else if (ch == "3") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"IP ADDRESS\""
                                  " --inputbox \"请输入 ip 地址 (默认 127.0.0.1)\" 10 50 \"127.0.0.1\"";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell("sed -i 's@XSDL_IP=.*@XSDL_IP=" + val + "@' " + script + " 2>/dev/null || true");
            } else if (ch == "4") {
                Executor::passthrough("${EDITOR:-nano} " + script + " 2>/dev/null || nano " + script);
            } else if (ch == "5") {
                // DISPLAY switch
                std::string cmd = cfg_.tui_bin +
                                  " --yesno \"是否切换 DISPLAY 转发开关？\" 0 0";
                auto r = Executor::passthrough(cmd);
                if (r.exit_code == 0)
                    Executor::shell(
                        "if grep -q '^export.*DISPLAY' " + script + "; then sed -i '/export DISPLAY=/d' " + script +
                        "; else echo 'export DISPLAY=:0' >> " + script + "; fi 2>/dev/null || true");
            } else if (ch == "6") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"VcXsrv DISPLAY PORT\""
                                  " --inputbox \"请输入 VcXsrv 显示端口 (默认 :0)\" 10 50 \":0\"";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(
                        "sed -i 's@VCXSRV_DISPLAY=.*@VCXSRV_DISPLAY=" + val + "@' " + script + " 2>/dev/null || true");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_xrdp_menu() {
        // 对应旧 Bash modify_xrdp_conf: 先 proot检查 + Start/Configure yesno
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            Logger::warn("检测到 proot 容器环境，xrdp 可能无法正常连接！");
            auto entry_r = Executor::passthrough(cfg_.tui_bin +
                                                 " --yesno \"检测到 proot 容器环境，xrdp 可能无法正常连接，是否继续？\" 0 0");
            if (entry_r.exit_code != 0) return;
        }

        auto status_r = Executor::shell("pgrep xrdp 2>/dev/null");
        bool is_running = status_r.ok() && !status_r.stdout_data.empty();

        auto entry = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"你想要对这个小可爱做什么\""
                                           " --yes-button \"" + std::string(is_running ? "Restart 重启" : "Start 启动") +
                                           "\""
                                           " --no-button \"Configure 配置\""
                                           " --yesno \"您是想要启动服务还是配置服务？检测到 xrdp 进程" +
                                           std::string(is_running ? "正在运行" : "未运行") + "\" 9 50");

        if (entry.exit_code == 0) {
            if (!fs::exists("/usr/sbin/xrdp")) {
                Logger::warn("未检测到 xrdp，正在初始化配置...");
                install_xrdp();
                configure_xrdp_desktop();
            }
            Executor::passthrough("service xrdp restart 2>/dev/null || "
                "systemctl restart xrdp 2>/dev/null || /usr/sbin/xrdp 2>/dev/null &");
            Logger::ok("xrdp 已" + std::string(is_running ? "重启" : "启动"));
            Logger::press_enter();
            return;
        }

        // Configure: 进入 11 项子菜单
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"CONFIGURE XRDP\""
                               " --menu \"Type service xrdp start to start it\" 0 0 0 "
                               "\"1\"  \"One-key conf 初始化一键配置\" "
                               "\"2\"  \"指定xrdp桌面环境\" "
                               "\"3\"  \"xrdp port 修改xrdp端口\" "
                               "\"4\"  \"xrdp.ini 修改配置文件\" "
                               "\"5\"  \"startwm.sh 修改启动脚本\" "
                               "\"6\"  \"stop 停止\" "
                               "\"7\"  \"status 进程状态\" "
                               "\"8\"  \"pulse_server 音频服务\" "
                               "\"9\"  \"reset 重置\" "
                               "\"10\" \"remove 卸载/移除\" "
                               "\"11\" \"进程管理说明\" "
                               "\"0\"  \"" + std::string(_("menu.tui.back")) + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") {
                Executor::passthrough("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || true");
                install_xrdp();
                configure_xrdp_desktop();
            } else if (ch == "2") configure_xrdp_desktop();
            else if (ch == "3") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"xrdp port\""
                                  " --inputbox \"请输入 xrdp 端口 (默认 3389)\" 10 40 \"3389\"";
                std::string val = Executor::tui_select(cmd);
                Logger::debug(val);
                if (!val.empty()) {
                    try { set_xrdp_port(std::stoi(val)); } catch (...) {
                    }
                }
            } else if (ch == "4") {
                Executor::shell("mkdir -p /etc/xrdp 2>/dev/null || true");
                Executor::passthrough("${EDITOR:-nano} /etc/xrdp/xrdp.ini 2>/dev/null || nano /etc/xrdp/xrdp.ini");
            } else if (ch == "5") {
                Executor::shell("mkdir -p /etc/xrdp 2>/dev/null || true");
                Executor::passthrough("${EDITOR:-nano} /etc/xrdp/startwm.sh 2>/dev/null || nano /etc/xrdp/startwm.sh");
            } else if (ch == "6") stop_xrdp();
            else if (ch == "7") {
                auto r = Executor::shell("ps aux | grep xrdp | grep -v grep 2>/dev/null");
                if (!r.stdout_data.empty()) {
                    Logger::info("xrdp 进程正在运行:");
                    Logger::info(r.stdout_data);
                } else {
                    Logger::info("xrdp 进程未运行");
                }
            } else if (ch == "8") {
                std::string cmd = cfg_.tui_bin +
                                  " --title \"PULSE SERVER\""
                                  " --inputbox \"请输入 PulseAudio 服务器地址\" 10 50 \"127.0.0.1\"";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty())
                    Executor::shell(
                        "sed -i 's@PULSE_SERVER=.*@PULSE_SERVER=" + val +
                        "@' /etc/xrdp/startwm.sh 2>/dev/null || true");
            } else if (ch == "9") {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认重置 xrdp 配置？\" 0 0");
                if (r.exit_code == 0) {
                    Executor::passthrough("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || true");
                    Executor::passthrough("rm -rf /etc/xrdp/xrdp.ini /etc/xrdp/startwm.sh 2>/dev/null || true");
                    install_xrdp();
                    configure_xrdp_desktop();
                }
            } else if (ch == "10") {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 xrdp？\" 0 0");
                if (r.exit_code == 0) remove_xrdp();
            } else if (ch == "11") {
                Logger::info("xrdp 进程管理:");
                Logger::info("  启动: service xrdp start 或 systemctl start xrdp");
                Logger::info("  停止: service xrdp stop 或 systemctl stop xrdp");
                Logger::info("  重启: service xrdp restart 或 systemctl restart xrdp");
                Logger::info("  状态: service xrdp status 或 systemctl status xrdp");
            }
            Logger::press_enter();
        }
    }

    void GUIManager::configure_remote_desktop_environment(std::string_view context) {
        // 对应旧 Bash configure_remote_desktop_environment (gui:4918-4996)
        // context: "x11vnc" or "xrdp"
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"REMOTE_DESKTOP\""
                               " --menu \"您想要配置哪个桌面？按方向键选择，回车键确认！\\n Which desktop environment do you want to configure? \" 0 0 0 "
                               "\"1\" \"auto 自动选择\" "
                               "\"2\" \"xfce：兼容性高\" "
                               "\"3\" \"lxde：轻量化桌面\" "
                               "\"4\" \"mate：基于GNOME 2\" "
                               "\"5\" \"lxqt\" "
                               "\"6\" \"kde plasma 5\" "
                               "\"7\" \"gnome 3\" "
                               "\"8\" \"cinnamon\" "
                               "\"9\" \"dde (deepin desktop)\" "
                               "\"0\" \"我一个都不选 =￣ω￣=\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice == "0" || choice.empty()) return;

        std::string session_01, session_02;
        if (choice == "1") {
            session_01 = "/etc/X11/xinit/Xsession";
            session_02 = "/etc/X11/xinit/Xsession";
        } else if (choice == "2") {
            session_01 = "xfce4-session";
            session_02 = "startxfce4";
        } else if (choice == "3") {
            session_01 = "lxsession";
            session_02 = "startlxde";
        } else if (choice == "4") {
            session_01 = "mate-session";
            session_02 = "mate-panel";
        } else if (choice == "5") {
            session_01 = "startlxqt";
            session_02 = "lxqt-session";
        } else if (choice == "6") {
            session_01 = "startplasma-x11";
            session_02 = "startkde";
        } else if (choice == "7") {
            session_01 = "gnome-session";
            session_02 = "gnome-panel";
        } else if (choice == "8") {
            session_01 = "cinnamon-session";
            session_02 = "cinnamon-launcher";
        } else if (choice == "9") {
            session_01 = "startdde";
            session_02 = "dde-launcher";
        } else return;

        // 检测哪个会话命令可用
        std::string remote_session;
        if (Executor::has(session_01)) {
            remote_session = session_01;
        } else {
            remote_session = session_02;
        }

        Logger::info("远程桌面会话: " + remote_session);

        // 根据上下文调用相应的完整配置函数
        std::string ctx(context);
        if (ctx == "xrdp") {
            configure_xrdp_remote_desktop_session(remote_session);
            // 旧 Bash configure_xrdp_remote_desktop_session 末尾会重启, 这里补上
            xrdp_restart();
        } else {
            // x11vnc
            configure_x11vnc_remote_desktop_session();
        }
    }

    void GUIManager::configure_xrdp_desktop() {
        // 对应旧 Bash xrdp_desktop_environment → configure_remote_desktop_environment
        configure_remote_desktop_environment("xrdp");
    }

    void GUIManager::configure_x11vnc_remote_desktop_session() {
        // 对应旧 Bash configure_x11vnc_remote_desktop_session (gui:1194-1213)
        Logger::step(_("gui.configuring_x11vnc_session"));

        // 1. 部署 startx11vnc 和 x11vncpasswd 脚本
        Executor::shell("cd /usr/local/bin/ && "
            "rm -f startx11vnc x11vncpasswd 2>/dev/null; "
            "cp -f ${TMOE_GIT_DIR:-/usr/local/etc/tmoe-linux/git}/share/old-version/tools/gui/startx11vnc "
            "${TMOE_GIT_DIR:-/usr/local/etc/tmoe-linux/git}/share/old-version/tools/gui/x11vncpasswd "
            "./ 2>/dev/null || true");
        Executor::shell("chmod a+rx /usr/local/bin/startx11vnc /usr/local/bin/x11vncpasswd 2>/dev/null || true");

        // 若 deploy_startup_scripts 已部署则跳过复制，但确保 x11passwd 存在
        if (fs::exists(vnc_manager_.config().passwd_file)) {
            Executor::shell("cd " + vnc_manager_.config().vnc_home_dir.string() + " && "
                            "cp -pvf passwd x11passwd 2>/dev/null || "
                            "cd /usr/local/bin && ./x11vncpasswd 2>/dev/null || true");
        } else {
            Executor::shell("cd /usr/local/bin && ./x11vncpasswd 2>/dev/null || "
                            "x11vnc -storepasswd " +
                            (vnc_manager_.config().vnc_home_dir / "x11passwd").string() + " 2>/dev/null || true");
        }

        Logger::info(_("gui.x11vnc_manager_.config()complete"));
        Logger::info("You can type startx11vnc to restart it, type stopvnc to stop it.");
        Logger::info(std::string(_("gui.switch_to_startvnc")));
    }

    void GUIManager::configure_xrdp_remote_desktop_session(const std::string &session_cmd) {
        // 对应旧 Bash configure_xrdp_remote_desktop_session (gui:4998-5023)
        Logger::step(std::string(_("gui.configuring_xrdp_session")) + " " + session_cmd);

        Executor::shell("mkdir -p /etc/xrdp 2>/dev/null");
        Executor::shell("cd /etc/xrdp && "
            "sed -i '/Xsession/d' startwm.sh 2>/dev/null; "
            "if grep -q 'exec' startwm.sh 2>/dev/null; then "
            "  sed -i '$ d' startwm.sh 2>/dev/null; "
            "  sed -i '$ d' startwm.sh 2>/dev/null; "
            "fi || true");

        // 添加 Xsession 行
        SystemHelper::append_file("/etc/xrdp/startwm.sh",
                                  "test -x /etc/X11/Xsession && exec /etc/X11/Xsession\n"
                                  "exec /etc/X11/xinit/Xsession\n");

        // 替换为 dbus-launch + 实际会话
        Executor::shell("sed -i 's@exec /etc/X11/Xsession@exec dbus-launch " + session_cmd +
                        "@g' /etc/xrdp/startwm.sh 2>/dev/null || true");

        Logger::info(std::string(_("gui.xrdp_session_config")));
        Executor::passthrough("cat /etc/xrdp/startwm.sh 2>/dev/null || true");

        // 不在此处重启: 让调用方控制重启时机，避免 xrdp.ini 未就绪时提前启动
        check_xrdp_status();
        Logger::ok(std::string(_("gui.xrdp_session_done")));
    }

    void GUIManager::run_beautification_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.beautify_title")) +
                                   "\" --menu \"" + std::string(_("gui.beautify_prompt")) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.beautify_gtk_theme")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.beautify_icon_theme")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.beautify_wallpaper")) + "\" "
                                   "\"4\" \"" + std::string(_("gui.beautify_plank")) + "\" "
                                   "\"5\" \"" + std::string(_("gui.beautify_compiz")) + "\" "
                                   "\"6\" \"" + std::string(_("gui.beautify_conky")) + "\" "
                                   "\"7\" \"" + std::string(_("gui.beautify_cursor")) + "\" "
                                   "\"8\" \"" + std::string(_("gui.beautify_xfce_panel")) + "\" "
                                   "\"9\" \"" + std::string(_("gui.beautify_iosevka")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                std::string theme_menu = cfg_.tui_bin +
                                         " --title \"" + std::string(_("gui.gtk_theme_title")) +
                                         "\" --menu \"" + std::string(_("gui.gtk_theme_prompt")) + "\" 0 0 0 "
                                         "\"arc\" \"" + std::string(_("gui.gtk_theme_arc")) + "\" "
                                         "\"adapta\" \"" + std::string(_("gui.gtk_theme_adapta")) + "\" "
                                         "\"numix\" \"" + std::string(_("gui.gtk_theme_numix")) + "\" "
                                         "\"materia\" \"" + std::string(_("gui.gtk_theme_materia")) + "\" "
                                         "\"breeze\" \"" + std::string(_("gui.gtk_theme_breeze")) + "\"";
                std::string t = Executor::tui_select(theme_menu);
                if (!t.empty() && t != "0") install_theme(t);
            } else if (choice == "2") {
                std::string icon_menu = cfg_.tui_bin +
                                        " --title \"" + std::string(_("gui.icon_theme_title")) +
                                        "\" --menu \"" + std::string(_("gui.icon_theme_prompt")) + "\" 0 0 0 "
                                        "\"papirus\" \"" + std::string(_("gui.icon_papirus")) + "\" "
                                        "\"numix\" \"" + std::string(_("gui.icon_numix")) + "\" "
                                        "\"breeze\" \"" + std::string(_("gui.icon_breeze")) + "\" "
                                        "\"elementary\" \"" + std::string(_("gui.icon_elementary")) + "\" "
                                        "\"tango\" \"" + std::string(_("gui.icon_tango")) + "\" "
                                        "\"moka\" \"" + std::string(_("gui.icon_moka")) + "\" "
                                        "\"faenza\" \"" + std::string(_("gui.icon_faenza")) + "\"";
                std::string i = Executor::tui_select(icon_menu);
                if (!i.empty() && i != "0") install_icon_theme(i);
            } else if (choice == "3") {
                std::string wp_menu = cfg_.tui_bin +
                                      " --title \"" + std::string(_("gui.wallpaper_title")) +
                                      "\" --menu \"" + std::string(_("gui.wallpaper_prompt")) + "\" 0 0 0 "
                                      "\"gnome\" \"" + std::string(_("gui.wp_gnome")) + "\" "
                                      "\"xfce\" \"" + std::string(_("gui.wp_xfce")) + "\" "
                                      "\"mate\" \"" + std::string(_("gui.wp_mate")) + "\" "
                                      "\"deepin\" \"" + std::string(_("gui.wp_deepin")) + "\" "
                                      "\"kde\" \"" + std::string(_("gui.wp_kde")) + "\"";
                std::string wp = Executor::tui_select(wp_menu);
                if (!wp.empty() && wp != "0") download_wallpaper(wp);
            } else if (choice == "4") {
                install_dock();
            } else if (choice == "5") {
                install_compiz();
            } else if (choice == "6") {
                install_conky();
            } else if (choice == "7") {
                std::string cursor_menu = cfg_.tui_bin +
                                          " --title \"" + std::string(_("gui.cursor_title")) +
                                          "\" --menu \"" + std::string(_("gui.cursor_prompt")) + "\" 0 0 0 "
                                          "\"breeze\" \"" + std::string(_("gui.cursor_breeze")) + "\" "
                                          "\"chameleon\" \"" + std::string(_("gui.cursor_chameleon")) + "\"";
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
        // 对应旧 Bash docker_auto_install_gui_env (gui:57-118)
        Logger::step(_("gui.docker_env_prep"));

        // 创建 tmoe/tome 软链接
        Executor::shell("ln -sfv ${TMOE_GIT_DIR:-/usr/local/etc/tmoe-linux/git}/share/old-version/share/app/tmoe "
            "/usr/local/bin/tmoe 2>/dev/null || true");
        Executor::shell("ln -svf tmoe /usr/local/bin/tome 2>/dev/null || true");

        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        bool is_debian = (family == DistroFamily::Debian);
        bool is_ubuntu = (is_debian && cfg_.sub_distro == "ubuntu");
        bool is_kali = (is_debian && cfg_.sub_distro == "kali");

        // 安装 add-apt-repository (debian)
        if (is_debian && !Executor::has("add-apt-repository") && Executor::has("apt-get")) {
            Executor::passthrough("apt install -y software-properties-common 2>/dev/null || true");
        }

        // 安装 aria2c
        if (!Executor::has("aria2c")) {
            Executor::passthrough(cfg_.install_command + " aria2 2>/dev/null || " +
                                  cfg_.install_command + " aria2 2>/dev/null || true");
        }

        // Ubuntu: language-pack-zh
        if (is_ubuntu) {
            Executor::passthrough("apt install -y ^language-pack-zh 2>/dev/null || true");
        }

        // Kali: debian-archive-keyring
        if (is_kali) {
            Executor::passthrough("apt install -y debian-archive-keyring 2>/dev/null || true");
        }

        // 下载 Iosevka 字体
        desktop_manager_.download_iosevka_ttf_font_ext();

        // preconfigure_gui_dependecies_02
        desktop_manager_.preconfigure_gui_dependencies();

        // 按发行版设置自动安装标志 (对应旧 Bash docker_auto_install_gui_env)
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
            Executor::passthrough(cfg_.install_command + " baobab 2>/dev/null || true");
            Executor::passthrough(cfg_.install_command + " bleachbit 2>/dev/null || true");
            av = true;
        }

        // Kali tools
        if (is_kali) {
            kt = "kali-linux-arm";
            ak = true;
        }

        ac = true;
        desktop_manager_.set_auto_install_flags(af, ae, av, ac, ak, kt);

        // 创建 ~/.vnc 目录和 passwd 占位文件
        Executor::shell("mkdir -p ~/.vnc 2>/dev/null");
        Executor::shell("printf 'please delete the invalid passwd file\\n' > ~/.vnc/passwd 2>/dev/null || true");

        Logger::ok(_("gui.docker_env_done"));
    }

    bool GUIManager::auto_install_gui(std::string_view desktop) {
        Logger::info(_f("gui.auto_install.mode", std::string(desktop)));

        // 启用静默模式 (对应旧 Bash AUTO_INSTALL_GUI=true)
        set_auto_install_mode(true);

        // 1. Docker 自动安装环境准备 (旧 Bash docker_auto_install_gui_env)
        docker_auto_install_gui_env(desktop);

        // 2. 安装桌面 + 配置 VNC xstartup + first_configure_vnc + 追问链
        if (!desktop_manager_.install_desktop(desktop)) {
            Logger::error("桌面环境安装失败");
            return false;
        }

        // 3. 安装输入法
        desktop_manager_.install_fcitx();

        // 4. 修复权限 + dbus + 部署启动脚本
        vnc_manager_.fix_vnc_permissions();
        vnc_manager_.deploy_startup_scripts();
        vnc_manager_.fix_vnc_dbus();

        // 5. 启动 VNC
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

        // 检查是否已安装
        auto check = Executor::shell("fc-list | grep -qi iosevka && echo 'yes'");
        if (check.stdout_data.find("yes") != std::string::npos) {
            Logger::ok("Iosevka 字体已安装");
            return true;
        }

        // 先尝试包管理器
        if (Executor::passthrough(cfg_.install_command + " fonts-iosevka 2>/dev/null").ok()) {
            Logger::ok("Iosevka 字体安装完成");
            return true;
        }

        // 备用: 从 GitHub 下载
        const char *iosevka_url = "https://github.com/be5invis/Iosevka/releases/download/v28.1.0/"
                "PkgTTF-Iosevka-28.1.0.zip";

        std::string font_dir = "/usr/local/share/fonts/iosevka";
        Executor::shell("mkdir -p " + font_dir);

        Logger::info("从 GitHub 下载 Iosevka...");
        std::string dl_cmd = "cd /tmp && (wget -q --timeout=30 '" + std::string(iosevka_url) +
                             "' -O iosevka.zip 2>/dev/null || "
                             "curl -sL '" + std::string(iosevka_url) + "' -o iosevka.zip 2>/dev/null)"
                             " && unzip -o iosevka.zip -d " + font_dir +
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
        // 对应旧 Bash first_configure_startvnc 中完整的 HiDPI 检测流程
        std::string wm_size_file = "/usr/local/etc/tmoe-linux/wm_size.txt";
        int orig_w = 0, orig_h = 0;
        bool high_dpi = false;

        // 1. 读取 wm_size.txt
        if (fs::exists(wm_size_file)) {
            auto content = SystemHelper::read_file(fs::path(wm_size_file));
            // 旧 Bash: awk -F 'x' '{print $2,$1}' → 宽<高时交换
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

        // 2. Termux: wm size
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

        // 3. Android 分辨率确认提示 (旧 Bash lines 5525-5533)
        if (orig_w > 0 && !cfg_.is_termux) {
            std::string res_str = std::to_string(orig_w) + "x" + std::to_string(orig_h);
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"Is your resolution " + res_str + "?\""
                                           " --yes-button 'YES' --no-button 'NO'"
                                           " --yesno \"Your host is detected as Android and the resolution is " +
                                           res_str + "\" 0 50");
            if (r.exit_code != 0) {
                orig_w = 0;
                orig_h = 0;
                high_dpi = false;
            } else {
                Logger::info("Your resolution is " + res_str);
            }
        }

        // 4. xfce 二次确认: 720P/1080P 或 2K/4K (旧 Bash lines 5539-5554)
        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (orig_w == 0 && (d.find("xfce") != std::string::npos)) {
            auto r2 = Executor::passthrough(cfg_.tui_bin +
                                            " --title \"720P/1080P Screen/Monitor\""
                                            " --yes-button 'Yes (720P~1080P)' --no-button 'No (2K~4K)'"
                                            " --yesno '若屏幕分辨率 x, 720P<=x<=1080P 选 Yes, 2K<=x<=4K 选 No' 0 50");
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

        // 5. LXDE: 重命名 lxpolkit 文件 (旧 Bash lines 5556-5563)
        if (d.find("lxde") != std::string::npos) {
            Executor::shell("for f in /etc/xdg/autostart/lxpolkit.desktop /usr/bin/lxpolkit; do "
                "[ -f \"$f\" ] && mv -f \"$f\" \"$f.bak\" 2>/dev/null; done || true");
        }

        // 6. 兜底默认分辨率
        if (orig_w == 0) {
            orig_w = 1440;
            orig_h = 720;
        }

        // 7. 应用 HiDPI 设置 (对应旧 Bash case TMOE_HIGH_DPI)
        if (high_dpi) {
            Logger::info("Tmoe-linux tool将为您自动调整高分屏设定");
            Executor::shell(
                "dbus-launch xfconf-query -c xsettings -t int -np /Gdk/WindowScalingFactor -s 2 2>/dev/null || true");
            auto focal_check = Executor::shell("grep -q 'Focal Fossa' /etc/os-release && echo 'yes'");
            if (focal_check.ok() && focal_check.stdout_data.find("yes") != std::string::npos) {
                Executor::shell("xfconf-query -c xfwm4 -p /general/theme -s Kali-Light-xHiDPI 2>/dev/null || true");
            } else {
                Executor::shell("xfconf-query -c xfwm4 -p /general/theme -s Default-xhdpi 2>/dev/null || true");
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

    bool GUIManager::stop_novnc() {
        Logger::step("停止 noVNC...");
        Executor::shell("pkill -f websockify 2>/dev/null || true");
        Logger::ok("noVNC websockify 已停止");
        return true;
    }

    bool GUIManager::remove_novnc() {
        Logger::step("卸载 noVNC...");
        stop_novnc();
        // 清理 pip3 安装的包
        Executor::passthrough("pip3 uninstall -y websockify numpy 2>/dev/null || true");
        // 清理目录
        Executor::shell("rm -rf /opt/novnc /usr/share/novnc 2>/dev/null || true");
        Executor::passthrough(cfg_.remove_command + " novnc websockify 2>/dev/null || true");
        Logger::ok("noVNC 已卸载");
        return true;
    }

    bool GUIManager::download_wallpaper(std::string_view source) {
        Logger::step("下载壁纸: " + std::string(source));

        std::string wallpaper_dir = "/usr/share/backgrounds/tmoe";
        Executor::shell("mkdir -p " + wallpaper_dir);

        std::string url;
        std::string filename;
        std::string src_lower(source);
        std::transform(src_lower.begin(), src_lower.end(), src_lower.begin(), ::tolower);

        if (src_lower == "debian" || src_lower == "gnome") {
            SystemHelper::install_packages({"gnome-backgrounds"}, cfg_.install_command);
            Logger::ok("GNOME 壁纸包已安装");
            return true;
        } else if (src_lower == "xfce" || src_lower == "xubuntu") {
            url = "https://gitlab.xfce.org/artwork/xfce4-artwork/-/raw/master/backgrounds/xfce-stripes.png";
            filename = "xfce-stripes.png";
        } else if (src_lower == "mate" || src_lower == "ubuntu-mate") {
            SystemHelper::install_packages({"ubuntu-mate-wallpapers"}, cfg_.install_command);
            Logger::ok("Ubuntu MATE 壁纸包已安装");
            return true;
        } else if (src_lower == "deepin") {
            SystemHelper::install_packages({"deepin-wallpapers"}, cfg_.install_command);
            Logger::ok("Deepin 壁纸包已安装");
            return true;
        } else if (src_lower == "kde") {
            SystemHelper::install_packages({"plasma-workspace-wallpapers"}, cfg_.install_command);
            Logger::ok("KDE 壁纸包已安装");
            return true;
        } else {
            Logger::info("未知壁纸源: " + std::string(source) + "，跳过下载");
            return false;
        }

        if (!url.empty()) {
            Executor::passthrough("wget -q '" + url + "' -O " + wallpaper_dir + "/" + filename +
                                  " 2>/dev/null || curl -sL '" + url + "' -o " + wallpaper_dir + "/" + filename +
                                  " 2>/dev/null");
            set_wallpaper(wallpaper_dir + "/" + filename);
        }

        Logger::ok("壁纸下载完成");
        return true;
    }

    bool GUIManager::install_conky() {
        Logger::step("安装 Conky 系统监控...");
        if (!SystemHelper::install_packages({"conky", "conky-all"}, cfg_.install_command)) {
            Logger::warn("Conky 安装失败");
            return false;
        }

        // 写入基础 conky 配置
        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        std::string conky_dir = home + "/.config/conky";
        Executor::shell("mkdir -p " + conky_dir);

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

        // 创建 autostart
        std::string autostart = home + "/.config/autostart/conky.desktop";
        std::string desktop_entry =
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Conky\n"
                "Exec=conky -c " + conky_dir + "/conky.conf\n"
                "X-GNOME-Autostart-enabled=true\n";
        SystemHelper::write_file(fs::path(autostart), desktop_entry);

        Logger::ok("Conky 安装完成");

        // 尝试克隆 Harmattan conky 主题 (对应旧 Bash configure_conky)
        if (!fs::exists(home + "/github/Harmattan")) {
            Executor::shell("mkdir -pv " + home + "/github && cd " + home + "/github && "
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

        if (!SystemHelper::install_packages(pkgs, cfg_.install_command)) {
            Logger::warn("部分 Compiz 组件安装失败，尝试核心包...");
            SystemHelper::install_packages({"compiz", "compiz-core", "compiz-plugins"}, cfg_.install_command);
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
        if (!Executor::passthrough(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok()) {
            Logger::warn("鼠标指针安装可能失败，请手动检查");
            return false;
        }
        Logger::ok("鼠标指针主题安装完成");
        return true;
    }

    bool GUIManager::deploy_xfce_panel_config() {
        // 仅在 XFCE 环境下部署
        Logger::step("部署 XFCE4 面板配置...");

        std::string home = std::getenv("HOME") ? std::string(std::getenv("HOME")) : "/root";
        fs::path panel_dir = fs::path(home) / ".config" / "xfce4" / "xfconf" / "xfce-perchannel-xml";
        Executor::shell("mkdir -p " + panel_dir.string());

        std::string panel_xml = generate_xfce_panel_xml();
        return SystemHelper::write_file(panel_dir / "xfce4-panel.xml", panel_xml);
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
        std::string menu = cfg_.tui_bin + " --title \"UBUNTU壁纸\" --menu \"Download ubuntu wallpaper-packs\" 0 50 0 ";
        for (int i = 0; codes[i]; ++i)
            menu += "\"" + std::to_string(i + 1) + "\" \"" + codes[i] + "\" ";
        menu += "\"0\" \"Back\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < 22) download_ubuntu_wallpaper(codes[idx]);
    }


    void GUIManager::x11vnc_onekey() {
        x11vnc_warning();
        // configure_remote_desktop_environment for x11vnc
        configure_remote_desktop_environment("x11vnc");
    }


    void GUIManager::download_uos_icon_theme() {
        Executor::passthrough(cfg_.install_command + " deepin-icon-theme 2>/dev/null || true");
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
        Executor::shell("rm -rf /tmp/UosICONS 2>/dev/null || true");
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
        std::string url_cmd = cfg_.tui_bin +
                              " --title \"Tmoe xfce&gnome theme parser\""
                              " --inputbox \"请输入主题链接Please enter a url\\n例如https://gnome-look.org/xx或https://xfce-look.org/xx\" 0 50";
        std::string url = Executor::tui_select(url_cmd);
        if (url.empty()) return;

        // 下载 HTML 并解析主题列表
        Logger::info("正在解析主题页面...");
        Executor::shell("cd /tmp && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o .theme_index_cache_tmoe.html '" + url + "' 2>/dev/null || "
                        "curl -L -o .theme_index_cache_tmoe.html '" + url + "' 2>/dev/null || true");
        // 对应旧 Bash xfce_theme_parsing — 解析主题名和下载量
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


    void GUIManager::xrdp_onekey() {
        Logger::step("XRDP 一键配置...");
        // 安装 xrdp (按发行版)
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        if (!Executor::has("xrdp-keygen") && !fs::exists("/usr/sbin/xrdp")) {
            if (family == DistroFamily::Gentoo) {
                Executor::passthrough("emerge -avk layman 2>/dev/null; layman -a bleeding-edge 2>/dev/null; "
                    "layman -S 2>/dev/null || true");
            }
            Executor::passthrough(cfg_.install_command + " xrdp 2>/dev/null || true");
        }
        // 修复 xrdp 证书权限: key.pem 默认 640 root:ssl-cert
        Executor::shell(
            "if getent group ssl-cert >/dev/null 2>&1; then "
            "  usermod -a -G ssl-cert xrdp 2>/dev/null || true; "
            "fi; "
            "chown -R root:ssl-cert /etc/xrdp/ 2>/dev/null || true; "
            "chmod 640 /etc/xrdp/key.pem 2>/dev/null || true; "
            "chmod 644 /etc/xrdp/cert.pem 2>/dev/null || true");
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            Executor::shell("usermod -a -G aid_inet xrdp 2>/dev/null || true");
        }
        // polkit 规则
        Executor::shell("mkdir -pv /etc/polkit-1/localauthority.conf.d /etc/polkit-1/localauthority/50-local.d/");
        SystemHelper::write_file("/etc/polkit-1/localauthority.conf.d/02-allow-colord.conf",
                                 generate_polkit_colord_conf());
        SystemHelper::write_file("/etc/polkit-1/localauthority/50-local.d/45-allow.colord.pkla",
                                 generate_polkit_colord_pkla());
        // 备份配置
        if (!fs::exists(
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config/tmoe-linux/xrdp.ini")) {
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
            Executor::shell("mkdir -pv " + home + "/.config/tmoe-linux/; "
                            "cp -p /etc/xrdp/startwm.sh /etc/xrdp/xrdp.ini " + home +
                            "/.config/tmoe-linux/ 2>/dev/null || true");
        }
        // 修复 startwm.sh — 注入 WSL/WSLg/GPU 环境修复 (幂等插入)
        Executor::shell("sed -i 's@exec /etc/X11/Xsession@exec /etc/X11/xinit/Xsession@g;"
            "s:exec /bin/sh /etc/X11/Xsession:exec /etc/X11/xinit/Xsession:g' "
            "/etc/xrdp/startwm.sh 2>/dev/null || true");

        auto ensure_line = [](const std::string &pattern, const std::string &line, int at_line) {
            Executor::shell(
                "if ! grep -q '" + pattern + "' /etc/xrdp/startwm.sh 2>/dev/null; then "
                "  sed -i '" + std::to_string(at_line) + "i\\" + line + "' /etc/xrdp/startwm.sh 2>/dev/null || true; "
                "fi");
        };
        ensure_line("unset WAYLAND_DISPLAY", "unset WAYLAND_DISPLAY", 2);
        ensure_line("unset XDG_RUNTIME_DIR", "unset XDG_RUNTIME_DIR", 3);
        ensure_line("LIBGL_ALWAYS_SOFTWARE", "export LIBGL_ALWAYS_SOFTWARE=1", 4);
        ensure_line("GALLIUM_DRIVER", "export GALLIUM_DRIVER=llvmpipe", 5);
        ensure_line("^export PULSE_SERVER", "export PULSE_SERVER=127.0.0.1", 6);

        // WSL 处理
        if (cfg_.is_wsl) {
            Logger::info("WSL环境: 正在设置端口(避免与3389冲突)...");
            Executor::shell(
                "sed -i 's/^export PULSE_SERVER=.*/export PULSE_SERVER=$(ip route list table 0 | head -n 1 | "
                "awk -F '\"'\"'default via '\"'\"' \"'\"'{print $2}'\"'\"' |awk \"'\"'{print $1}'\"'\"')/' "
                "/etc/xrdp/startwm.sh 2>/dev/null || true");
        }
        xrdp_port();
        xrdp_restart();
        Logger::ok("XRDP 一键配置完成");
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
        Executor::shell("rm -rf /tmp/.WINDOWS_11_ICON_THEME 2>/dev/null || true");
        desktop_manager_.set_default_xfce_icon_theme("We10X-Valley");
    }


    void GUIManager::xubuntu_wallpapers_menu() {
        const char *items[] = {"xubuntu-trusty", "xubuntu-xenial", "xubuntu-bionic", "xubuntu-focal", nullptr};
        const char *folders[] = {
            "xubuntu-community-artwork/trusty", "xubuntu-community-artwork/xenial",
            "xubuntu-community-artwork/bionic", "xubuntu-community-artwork/focal"
        };
        std::string menu = cfg_.tui_bin + " --title \"桌面壁纸\" --menu \"您想要下载哪套xubuntu壁纸包？\" 0 50 0 ";
        for (int i = 0; items[i]; ++i)
            menu += "\"" + std::to_string(i + 1) + "\" \"" + items[i] + "\" ";
        menu += "\"0\" \"Back\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < 4) desktop_manager_.download_xubuntu_wallpaper(items[idx], folders[idx]);
    }


    void GUIManager::check_xrdp_status() {
        if (Executor::has("service")) {
            Executor::passthrough("service xrdp status 2>/dev/null | head -n 24");
        } else {
            Executor::passthrough("systemctl status xrdp 2>/dev/null | head -n 24");
        }
    }


    void GUIManager::install_gui() {
        // 对应旧 Bash install_gui (gui:356-387)
        Logger::step(_("gui.install_gui_title"));

        // WSL 检测
        if (cfg_.is_wsl) {
            Logger::info(_("gui.wsl_detected"));
            download_wsl_components();
        }

        // 检查 Iosevka 字体
        const char *iosevka_file = "/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf";
        if (!fs::exists(iosevka_file)) {
            desktop_manager_.download_iosevka_ttf_font_ext();
        }

        check_zstd();
        random_neko();

        // 下载预览图片
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

        // 进入桌面安装主菜单 (对应旧 Bash standand_desktop_installation)
        run_desktop_install_menu();
    }


    void GUIManager::download_arch_xfce_artwork() {
        Logger::step("下载 Arch XFCE artwork...");
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/extra/os/x86_64/";
        Executor::shell("cd /tmp && mkdir -pv .xfce_art && cd .xfce_art && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'xfce4-artwork' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo art.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf art.pkg.tar.xz -C / 2>/dev/null; rm -rf /tmp/.xfce_art || true");
    }


    void GUIManager::download_manjaro_wallpaper() {
        Logger::step("下载 Manjaro 壁纸...");
        Executor::shell("mkdir -pv /tmp/.manjaro_wp && cd /tmp/.manjaro_wp && "
            "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
            "-o 'wp2018.tar.xz' "
            "'https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/wallpapers-2018-1.2-1-any.pkg.tar.xz' 2>/dev/null && "
            "tar -Jxvf wp2018.tar.xz -C / 2>/dev/null && "
            "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
            "-o 'wp2017.tar.xz' "
            "'https://mirrors.bfsu.edu.cn/manjaro/pool/overlay/manjaro-sx-wallpapers-20171023-1-any.pkg.tar.xz' 2>/dev/null && "
            "tar -Jxvf wp2017.tar.xz -C / 2>/dev/null; rm -rf /tmp/.manjaro_wp || true");
    }


    void GUIManager::download_candy_icon_theme() {
        if (fs::exists("/usr/share/icons/candy-icons")) {
            Logger::info("检测到已安装 candy 图标主题");
            return;
        }
        Logger::step("下载 Candy 图标主题...");
        Executor::shell("mkdir -pv /tmp/.CANDY_ICON_THEME && cd /tmp/.CANDY_ICON_THEME && "
            "(curl -Lo master.zip https://ghproxy.com/https://github.com/EliverLara/candy-icons/"
            "archive/refs/heads/master.zip 2>/dev/null || "
            "curl -Lo master.zip https://github.com/EliverLara/candy-icons/"
            "archive/refs/heads/master.zip 2>/dev/null) && "
            "unzip master.zip 2>/dev/null && "
            "mv candy-icons-master /usr/share/icons/candy-icons 2>/dev/null && "
            "update-icon-caches /usr/share/icons/candy-icons 2>/dev/null &");
        Executor::shell("rm -rf /tmp/.CANDY_ICON_THEME 2>/dev/null || true");
        desktop_manager_.set_default_xfce_icon_theme("candy-icons");
    }


    void GUIManager::choose_vnc_port_5902_or_5903() {
        // 对应旧 Bash choose_vnc_port_5902_or_5903 (gui:5845-5859)
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"VNC PORT\" --yes-button \"5902\" --no-button \"5903\""
                                       " --yesno \"请选择VNC端口✨\\nPlease choose a vnc port\" 0 50");
        if (r.exit_code == 0) {
            vnc_manager_.config().display = 2;
        } else {
            vnc_manager_.config().display = 3;
        }
        vnc_manager_.config().update_port();
        Executor::shell("sed -i -E 's@(tmoe-linux) :.*@\\1 :" + std::to_string(vnc_manager_.config().display) +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
        Executor::shell("sed -i -E 's@(VNC_DISPLAY)=.*@\\1=" + std::to_string(vnc_manager_.config().display) +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
    }


    void GUIManager::check_zstd() {
        // 对应旧 Bash check_zstd: 确保 zstd 可用
        if (!Executor::has("zstd")) {
            Executor::passthrough(cfg_.install_command + " zstd 2>/dev/null || true");
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
        Executor::shell("mkdir -pv " + home + "/Pictures/ubuntu-wallpapers 2>/dev/null");
        std::string repo = "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-wallpapers/";
        Executor::shell("cd " + home + "/Pictures/ubuntu-wallpapers && "
                        "LATEST=$(curl -L '" + repo + "' 2>/dev/null | grep 'ubuntu-wallpapers-" + ubuntu_code + "' | "
                        "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-o ubuntu-wallpaper.deb '" + repo + "'\"$LATEST\" && "
                        "(ar xv ubuntu-wallpaper.deb 2>/dev/null; tar -Jxvf data.tar.xz -C . 2>/dev/null) || true");
    }


    void GUIManager::random_neko() {
        // 对应旧 Bash random_neko: 随机猫娘问候
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
        // 对应旧 Bash catimg_preview_lxde_mate_xfce_01 + _02 (gui:327-355)
        std::string lxde_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/BUSYeSLZRqq3i3oM.png"
                                   : "https://gitee.com/ak2/icons/raw/master/raspbian-lxde.jpg";
        std::string mate_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/1frRp1lpOXLPz6mO.jpg"
                                   : "https://gitee.com/ak2/icons/raw/master/ubuntu-mate.jpg";
        std::string xfce_url = cfg_.is_wsl
                                   ? "https://gitee.com/mo2/pic_api/raw/test/2020/03/15/a7IQ9NnfgPckuqRt.jpg"
                                   : "https://gitee.com/ak2/icons/raw/master/debian-xfce.jpg";

        // LXDE preview
        if (!fs::exists("/tmp/LXDE_BUSYeSLZRqq3i3oM.png"))
            Executor::shell("cd /tmp && curl -sLo 'LXDE_BUSYeSLZRqq3i3oM.png' '" + lxde_url + "' 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/LXDE_BUSYeSLZRqq3i3oM.png"))
            Executor::passthrough("catimg /tmp/LXDE_BUSYeSLZRqq3i3oM.png 2>/dev/null || true");

        // MATE preview
        if (!fs::exists("/tmp/MATE_1frRp1lpOXLPz6mO.jpg"))
            Executor::shell("cd /tmp && curl -sLo 'MATE_1frRp1lpOXLPz6mO.jpg' '" + mate_url + "' 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/MATE_1frRp1lpOXLPz6mO.jpg"))
            Executor::passthrough("catimg /tmp/MATE_1frRp1lpOXLPz6mO.jpg 2>/dev/null || true");

        // XFCE preview
        if (!fs::exists("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg"))
            Executor::shell("cd /tmp && curl -sLo 'XFCE_a7IQ9NnfgPckuqRt.jpg' '" + xfce_url + "' 2>/dev/null || true");
        if (Executor::has("catimg") && fs::exists("/tmp/XFCE_a7IQ9NnfgPckuqRt.jpg"))
            Executor::passthrough("catimg /tmp/XFCE_a7IQ9NnfgPckuqRt.jpg 2>/dev/null || true");

        // WSL: copy XFCE preview to VcXsrv dir
        if (cfg_.is_wsl) {
            Executor::shell(
                "[ -e '/mnt/c/Users/Public/Downloads/VcXsrv' ] || mkdir -p '/mnt/c/Users/Public/Downloads/VcXsrv'; "
                "cp -f /tmp/XFCE_a7IQ9NnfgPckuqRt.jpg '/mnt/c/Users/Public/Downloads/VcXsrv/' 2>/dev/null || true");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 7: x11vnc / XRDP 增强
    // ═══════════════════════════════════════════════════════════════


    void GUIManager::xrdp_port() {
        std::string current_port = Executor::shell("cat /etc/xrdp/xrdp.ini 2>/dev/null | grep 'port=' | head -n 1 | "
            "cut -d '=' -f 2").stdout_data;
        while (!current_port.empty() && (current_port.back() == '\n' || current_port.back() == '\r'))
            current_port.
                    pop_back();
        std::string cmd = cfg_.tui_bin +
                          " --title \"PORT\""
                          " --inputbox \"请输入新的端口号(纯数字)，范围在1-65525之间, 当前端口为" + current_port + "\\n"
                          "Please type the port number.\" 12 50 \"" + (current_port.empty() ? "3389" : current_port) +
                          "\"";
        std::string val = Executor::tui_select(cmd);
        if (!val.empty()) {
            set_xrdp_port(std::stoi(val));
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 8: 兼容性表格 / 警告 / 成就系统
    // ═══════════════════════════════════════════════════════════════


    void GUIManager::tmoe_theme_installer(const std::string &file_path, bool is_icon) {
        std::string extract_path = is_icon ? "/usr/share/icons" : "/usr/share/themes";
        Executor::shell("mkdir -p " + extract_path);
        Executor::passthrough("tar -xf '" + file_path + "' -C " + extract_path + " 2>/dev/null || "
                              "tar -Jxf '" + file_path + "' -C " + extract_path + " 2>/dev/null || true");
        Logger::ok("主题已安装到 " + extract_path);
    }


    void GUIManager::xrdp_restart() {
        std::string rdp_port = Executor::shell("cat /etc/xrdp/xrdp.ini 2>/dev/null | grep 'port=' | head -n 1 | "
            "cut -d '=' -f 2").stdout_data;
        while (!rdp_port.empty() && (rdp_port.back() == '\n' || rdp_port.back() == '\r')) rdp_port.pop_back();
        Executor::passthrough("service xrdp restart 2>/dev/null || systemctl restart xrdp 2>/dev/null || "
            "/etc/init.d/xrdp restart 2>/dev/null || true");
        check_xrdp_status();
        Logger::info("XRDP 端口: " + rdp_port);
        Logger::info("本地访问地址: localhost:" + rdp_port);
        if (cfg_.is_wsl) {
            Logger::info("WSL 环境: 正在启动音频服务...");
            Executor::shell("cd '/mnt/c/Users/Public/Downloads/pulseaudio/bin' 2>/dev/null && "
                "/mnt/c/WINDOWS/system32/cmd.exe /c \"start .\\pulseaudio.bat\" 2>/dev/null &");
        }
    }


    void GUIManager::download_wallpapers_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"桌面壁纸\""
                               " --menu \"您想要下载哪套壁纸包？\\n Which wallpaper-pack do you want to download? \" 0 50 0 "
                               "\"1\" \"ubuntu:汇聚了官方及社区的绝赞壁纸包\" "
                               "\"2\" \"Mint:聆听自然的律动与风之呼吸,感受清新而唯美\" "
                               "\"3\" \"deepin-community+official 深度\" "
                               "\"4\" \"elementary(如沐春风)\" "
                               "\"5\" \"raspberrypi pixel树莓派(美如画卷)\" "
                               "\"6\" \"manjaro-2017+2018\" "
                               "\"7\" \"gnome-backgrounds(简单而纯粹)\" "
                               "\"8\" \"xfce-artwork\" "
                               "\"9\" \"arch(领略别样艺术)\" "
                               "\"0\" \"🌚 Return to previous menu 返回上级菜单\"";
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
            std::string menu = cfg_.tui_bin +
                               " --title \"图标包\""
                               " --menu \"您想要下载哪个图标包？\\n Which icon-theme do you want to download? \" 0 50 0 "
                               "\"1\" \"win11:更新颖的UI设计\" "
                               "\"2\" \"candy:Sweet gradient icons\" "
                               "\"3\" \"pixel:raspberrypi树莓派\" "
                               "\"4\" \"paper:简约、灵动、现代化的图标包\" "
                               "\"5\" \"papirus:优雅的图标包,基于paper\" "
                               "\"6\" \"numix:modern现代化\" "
                               "\"7\" \"moka:简约一致的美学\" "
                               "\"8\" \"UOS\" "
                               "\"0\" \"🌚 Return to previous menu 返回上级菜单\"";
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
            Executor::shell("cd /tmp && curl -sLo '" + filename + "' '" + url + "' 2>/dev/null || true");
        }
        if (Executor::has("catimg") && fs::exists("/tmp/" + filename)) {
            Executor::passthrough("catimg /tmp/" + filename + " 2>/dev/null || true");
        }
    }


    void GUIManager::download_wsl_components() {
        if (!cfg_.is_wsl) return;
        Logger::step(_("gui.wsl_components_download"));

        // 对应旧 Bash tools/gui/wsl (75行): 用 aria2c/curl 下载预编译 tar 包，非 git clone
        std::string pub_dl = "/mnt/c/Users/Public/Downloads";
        Executor::shell("mkdir -p " + pub_dl + "/pulseaudio " + pub_dl + "/VcXsrv " + pub_dl + "/tigervnc 2>/dev/null");

        // 1. PulseAudio for Windows (预编译二进制)
        Logger::info("下载 Windows PulseAudio...");
        Executor::shell("cd " + pub_dl + "/pulseaudio && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/pulseaudio/pulseaudio.tar.xz' -o pulseaudio.tar.xz 2>/dev/null && "
                        "tar -Jxvf pulseaudio.tar.xz 2>/dev/null || true");

        // 2. VcXsrv X Server
        Logger::info("下载 Windows VcXsrv X Server...");
        Executor::shell("cd " + pub_dl + "/VcXsrv && "
                        "curl -LfsS 'https://gitee.com/mo2/wsl/raw/master/VcXsrv/VcXsrv.tar.xz' -o VcXsrv.tar.xz 2>/dev/null && "
                        "tar -Jxvf VcXsrv.tar.xz 2>/dev/null || true");

        // 3. TigerVNC Viewer for Windows
        Logger::info("下载 Windows TigerVNC Viewer...");
        Executor::shell("cd " + pub_dl + "/tigervnc && "
                        "curl -LfsS 'https://gitee.com/ak2/tigervnc-viewer/raw/master/vncviewer64.zip' -o vncviewer64.zip 2>/dev/null && "
                        "unzip -o vncviewer64.zip 2>/dev/null || true");

        Logger::ok(_("gui.wsl_components_done"));
    }


    void GUIManager::set_vnc_passwd() {
        // 对应旧 Bash set_vnc_passwd (gui:5828-5843) — 交互式密码设置
        vnc_manager_.configure_vnc_password();
    }


    void GUIManager::fix_vnc_dbus_launch() {
        // 对应旧 Bash --fix-dbus 标志
        vnc_manager_.fix_vnc_dbus();
        Logger::ok("VNC dbus 修复完成");
    }


    void GUIManager::ubuntu_wallpapers_and_photos_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"Ubuntu壁纸包\""
                               " --menu \"您想要下载哪套Ubuntu壁纸包？\" 0 50 0 "
                               "\"1\" \"ubuntu-gnome:(bionic,cosmic,etc.)\" "
                               "\"2\" \"xubuntu-community:(bionic,focal,etc.)\" "
                               "\"3\" \"ubuntu-mate\" "
                               "\"4\" \"ubuntu-kylin 优麒麟\" "
                               "\"0\" \"Back\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") ubuntu_gnome_wallpapers_menu();
            else if (ch == "2") xubuntu_wallpapers_menu();
            else if (ch == "3") desktop_manager_.download_ubuntu_mate_wallpaper();
            else if (ch == "4") download_ubuntu_kylin_wallpaper();
        }
    }


    void GUIManager::remove_x11vnc_ext() {
        Logger::step("停止并卸载 x11vnc...");
        vnc_manager_.stop_x11vnc();
        Executor::shell("rm -rfv /usr/local/bin/startx11vnc 2>/dev/null || true");
        Executor::passthrough(cfg_.remove_command + " x11vnc 2>/dev/null || true");
        Logger::ok("x11vnc 已卸载");
    }


    void GUIManager::x11vnc_warning() {
        Logger::info("------------------------");
        Logger::info(std::string(_("gui.vnc.x11vnc_intro")));
        Logger::info(std::string(_("gui.vnc.x11vnc_details")));
        Logger::info(std::string(_("gui.vnc.recommendation")));
        Logger::info(std::string(_("gui.vnc.performance_no_accel")));
        Logger::info(std::string(_("gui.vnc.performance_accel")));
        Logger::info(std::string(_("gui.vnc.configure_multiple")));
        Logger::info("------------------------");
    }


    void GUIManager::xrdp_pulse_server() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"MODIFY PULSE SERVER ADDRESS\""
                          " --inputbox \"输入 PulseAudio 服务器地址\\\\n当前为\"$(grep 'PULSE_SERVER' /etc/xrdp/startwm.sh | "
                          "grep -v '^#' | cut -d '=' -f 2 | head -n 1)\"\\\\n例如 127.0.0.1\" 15 50";
        std::string addr = Executor::tui_select(cmd);
        if (!addr.empty()) {
            Executor::shell("if ! grep -q '^export.*PULSE_SERVER' /etc/xrdp/startwm.sh; then "
                            "sed -i '1 a\\export PULSE_SERVER=" + addr + "' /etc/xrdp/startwm.sh; fi; "
                            "sed -i -E 's@(export PULSE_SERVER=).*@\\1" + addr +
                            "@' /etc/xrdp/startwm.sh 2>/dev/null || true");
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
        Executor::shell("mkdir -pv " + home + "/Pictures 2>/dev/null");
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
        download_raspbian_pixel_icon_theme(); // 复用
    }


    void GUIManager::local_theme_installer() {
        Logger::info("本地主题安装器");
        Logger::info("请将主题包 (.tar.gz / .tar.xz) 放置在 /tmp 目录");

        std::string cmd = cfg_.tui_bin +
                          " --title \"LOCAL THEME INSTALLER\""
                          " --inputbox \"请输入主题包文件名 (在 /tmp 目录下)\\n例如: my-theme.tar.xz\" 10 60";
        std::string filename = Executor::tui_select(cmd);

        if (filename.empty()) return;

        std::string full_path = "/tmp/" + filename;
        if (!fs::exists(full_path)) {
            Logger::error("文件不存在: " + full_path);
            return;
        }

        // 判断是主题还是图标
        auto type_r = Executor::passthrough(cfg_.tui_bin +
                                            " --title \"Please choose the file type\" --yes-button 'THEME主题' --no-button 'ICON图标包'"
                                            " --yesno '这是主题包还是图标包?' 0 50");
        bool is_icon = (type_r.exit_code == 1);

        tmoe_theme_installer(full_path, is_icon);
    }


    void GUIManager::x11vnc_doc() {
        Logger::info("x11vnc 文档: http://www.karlrunge.com/x11vnc/x11vnc_opts.html");
    }


    void GUIManager::do_you_want_to_configure_novnc() {
        // 对应旧 Bash do_you_want_to_configure_novnc (gui:5792-5826)
        vnc_manager_.check_the_which_command();
        Logger::info("You can type novnc to start novnc+websockify");
        Logger::info("配置完成后，您可以输novnc来启动novnc,在浏览器里输入novnc访问地址进行连接。");
        Logger::info("------------------------");
        Logger::info("Do you want to configure novnc?");
        Logger::info("您是否需要配置novnc？");

        // 对应旧 Bash: 先询问用户是否要配置 novnc
        auto confirm = Executor::passthrough(cfg_.tui_bin +
                                             " --yesno \"" + std::string(_("gui.confirm_novnc")) + "\" 0 0");
        if (confirm.exit_code != 0) {
            Logger::info(std::string(_("gui.skip_novnc")));
            return;
        }

        install_novnc();
        vnc_manager_.if_container_is_arm();

        // Achievement 解锁
        std::string tmoe_dir = "/usr/local/etc/tmoe-linux";
        if (!fs::exists(tmoe_dir + "/achievement01")) {
            Logger::info("Congratulations！恭喜您获得新成就: vnc大师");
            Logger::info("由于您获得了该成就，故解锁了本工具的vnc(所有可配置)选项。");
            Executor::shell("mkdir -p " + tmoe_dir + " && printf 'vnc master\\n' > " + tmoe_dir + "/achievement01");
        }
        std::string vnc_msg = Executor::has("apt-get")
                                  ? "您可以使用以下任意一条命令来启动vnc或x: \nstartvnc,tightvnc,tigervnc,startx11vnc,startxsdl,novnc,输入stopvnc停止"
                                  : "您可以使用以下任意一条命令来启动vnc或x: \nstartvnc,startx11vnc,startxsdl,novnc,输入stopvnc停止";
        Executor::passthrough(cfg_.tui_bin + " --title \"VNC COMMANDS\" --msgbox \"" + vnc_msg + "\" 12 55");
        Logger::info("*°▽°* You are a VNC Master！");
    }


    void GUIManager::download_elementary_wallpaper() {
        Logger::step("下载 Elementary 壁纸...");
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/pool/community/";
        Executor::shell("cd /tmp && mkdir -pv .elementary_wp && cd .elementary_wp && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'elementary-wallpapers' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo wp.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf wp.pkg.tar.xz -C / 2>/dev/null; rm -rf /tmp/.elementary_wp || true");
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
        Executor::shell("mkdir -pv /tmp/." + theme_name + " && cd /tmp/." + theme_name + " && "
                        "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'data.tar.xz' '" + url + "' 2>/dev/null || "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'data.tar.xz' '" + url_02 + "' 2>/dev/null) && "
                        "tar -Jxvf data.tar.xz -C / 2>/dev/null; rm -rf /tmp/." + theme_name + " || true");
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 4: FVWM 特殊安装 / DM systemctl
    // ═══════════════════════════════════════════════════════════════


    void GUIManager::download_arch_wallpaper() {
        link_to_debian_wallpaper();
        Logger::step("下载 Arch 壁纸...");
        std::string repo = "https://mirrors.bfsu.edu.cn/archlinux/pool/community/";
        Executor::shell("cd /tmp && mkdir -pv .arch_wp && cd .arch_wp && "
                        "curl -Lo index.html '" + repo + "' 2>/dev/null && "
                        "LATEST=$(cat index.html | grep 'archlinux-wallpaper' | grep pkg.tar | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                        "[ -n \"$LATEST\" ] && curl -Lo wp.pkg.tar.xz '" + repo + "'\"$LATEST\" && "
                        "tar -Jxvf wp.pkg.tar.xz -C / 2>/dev/null; rm -rf /tmp/.arch_wp || true");
    }


    void GUIManager::x11vnc_process_readme() {
        Logger::info("输startx11vnc启动x11vnc服务。");
        Logger::info("输stopvnc停止x11vnc");
        Logger::info("若您的音频服务端为Android系统，请输pulseaudio --start");
    }

    void GUIManager::xrdp_systemd() {
        Logger::info("systemd管理:");
        Logger::info("  systemctl start xrdp  启动");
        Logger::info("  systemctl stop xrdp   停止");
        Logger::info("  systemctl status xrdp 查看状态");
        Logger::info("  systemctl enable xrdp 开机自启");
        Logger::info("  systemctl disable xrdp 禁用自启");
        Logger::info("service命令:");
        Logger::info("  service xrdp start/stop/status");
        Logger::info("init.d管理:");
        Logger::info("  /etc/init.d/xrdp start/restart/stop/status/force-reload");
    }


    void GUIManager::configure_theme_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"桌面环境主题\""
                               " --menu \"您想要下载哪个主题？\\n Which theme do you want to download? \" 0 50 0 "
                               "\"1\" \"🌈 XFCE-LOOK-parser 主题链接解析器\" "
                               "\"2\" \"⚡ local-theme-installer 本地主题安装器\" "
                               "\"3\" \"🎭 win10:kali卧底模式主题\" "
                               "\"4\" \"🚥 MacOS:Mojave\" "
                               "\"5\" \"🍎 MacOS:Big Sur\" "
                               "\"6\" \"🎋 breeze:plasma桌面微风gtk+版主题\" "
                               "\"7\" \"Kali:Flat-Remix-Blue主题\" "
                               "\"8\" \"ukui:国产优麒麟ukui桌面主题\" "
                               "\"9\" \"arc:融合透明元素的平面主题\" "
                               "\"0\" \"🌚 Return to previous menu 返回上级菜单\"";
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
                Executor::passthrough(cfg_.install_command + " ukui-themes ukui-greeter 2>/dev/null || true");
                // 备用: 从 Debian 镜像下载 ukui-themes deb
                if (!fs::exists("/usr/share/icons/ukui-icon-theme-default") &&
                    !fs::exists("/usr/share/icons/ukui-icon-theme")) {
                    Executor::shell("mkdir -pv /tmp/.ukui-gtk-themes && cd /tmp/.ukui-gtk-themes && "
                        "UKUITHEME=\"$(curl -LfsS 'https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/' 2>/dev/null | "
                        "grep all.deb | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2)\" && "
                        "aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o 'ukui-themes.deb' \"https://mirrors.bfsu.edu.cn/debian/pool/main/u/ukui-themes/$UKUITHEME\" && "
                        "ar xv ukui-themes.deb && cd / && "
                        "tar -Jxvf /tmp/.ukui-gtk-themes/data.tar.xz ./usr 2>/dev/null && "
                        "update-icon-caches /usr/share/icons/ukui-icon-theme-basic "
                        "/usr/share/icons/ukui-icon-theme-classical "
                        "/usr/share/icons/ukui-icon-theme-default 2>/dev/null &");
                    Executor::shell("rm -rf /tmp/.ukui-gtk-themes 2>/dev/null || true");
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
            std::string menu = cfg_.tui_bin + " --title \"MINT壁纸包\" --menu \"Download Mint wallpaper-packs\" 0 50 0 ";
            for (int i = 0; codes[i]; ++i)
                menu += "\"" + std::to_string(i + 1) + "\" \"" + codes[i] + "\" ";
            menu += "\"0\" \"Back\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            int idx = std::stoi(ch) - 1;
            if (idx >= 0 && idx < 22) {
                desktop_manager_.download_mint_backgrounds(codes[idx]);
                Logger::press_enter();
            }
        }
    }


    void GUIManager::check_tmoe_linux_desktop_link() {
        // 对应旧 Bash check_tmoe_linux_desktop_link
        Executor::shell("ln -sfv ${TMOE_GIT_DIR:-/usr/local/etc/tmoe-linux/git}/share/old-version/share/app/tmoe "
            "/usr/local/bin/tmoe 2>/dev/null || true");
        Executor::shell("ln -svf tmoe /usr/local/bin/tome 2>/dev/null || true");
    }


    void GUIManager::xrdp_reset() {
        // 对应旧 Bash xrdp_reset (gui:5227-5238): 从备份恢复 xrdp 配置
        Logger::step("重置 XRDP 配置...");

        // 旧 Bash 有 do_you_want_to_continue 确认; 这里用 Logger::confirm
        if (!Logger::confirm("WARNING！继续执行此操作将丢失 xrdp 配置信息！\n确认重置？")) {
            return;
        }

        Executor::passthrough("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || "
            "pkill xrdp 2>/dev/null || true");

        Executor::shell("rm -f /etc/polkit-1/localauthority/50-local.d/45-allow.colord.pkla "
            "/etc/polkit-1/localauthority.conf.d/02-allow-colord.conf 2>/dev/null || true");

        // 尝试从备份恢复
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string backup_ini = home + "/.config/tmoe-linux/xrdp.ini";
        std::string backup_wm = home + "/.config/tmoe-linux/startwm.sh";

        bool restored = false;
        if (fs::exists(backup_ini) && fs::exists(backup_wm)) {
            // 旧 Bash: cd ~/.config/tmoe-linux && cp -pvf xrdp.ini startwm.sh /etc/xrdp/
            auto cp_result = Executor::shell("cp -pf " + backup_ini + " /etc/xrdp/xrdp.ini 2>/dev/null && "
                                             "cp -pf " + backup_wm + " /etc/xrdp/startwm.sh 2>/dev/null");
            if (cp_result.ok()) {
                restored = true;
                Logger::ok("已从备份恢复 xrdp.ini 和 startwm.sh");
            }
        }

        if (!restored) {
            // 备份不存在，重新生成配置而不是损坏现有文件
            Logger::warn("未找到 xrdp 备份，将重新生成配置...");
            // 先写 xrdp.ini (确保重启前配置已就绪)
            set_xrdp_port(3389);
            // 再生成 startwm.sh
            configure_xrdp_session("xfce");
            Logger::ok("已重新生成 xrdp 配置");
        }

        // 重新生成 polkit 规则
        Executor::shell(
            "mkdir -pv /etc/polkit-1/localauthority.conf.d /etc/polkit-1/localauthority/50-local.d/ 2>/dev/null");
        SystemHelper::write_file("/etc/polkit-1/localauthority.conf.d/02-allow-colord.conf",
                                 generate_polkit_colord_conf());
        SystemHelper::write_file("/etc/polkit-1/localauthority/50-local.d/45-allow.colord.pkla",
                                 generate_polkit_colord_pkla());

        xrdp_restart();
    }


    bool GUIManager::handle_gui_cli_flag(std::string_view flag) {
        // 对应旧 Bash gui_main CLI 路由 (gui:5-54)
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
        }
        // ── 方案B: thin wrapper CLI flags ──
        else if (flag == "--start-vnc") {
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
            start_novnc();
            return true;
        }
        // 默认: install_gui
        install_gui();
        return true;
    }


    void GUIManager::download_chameleon_cursor_theme() {
        // 下载 3 个光标主题
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


    std::string GUIManager::generate_polkit_colord_conf() {
        return gui_config::POLKIT_COLORD_CONF;
    }

    std::string GUIManager::generate_polkit_colord_pkla() {
        return gui_config::POLKIT_COLORD_PKLA;
    }
} // namespace tmoe::domain
