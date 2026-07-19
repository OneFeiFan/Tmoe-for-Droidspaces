#include "remote_desktop_manager.h"
#include "core/system_helper.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"
#include "core/str_utils.h"
#include "domain/system/package_manager.h"
#include "../gui_config/templates.h"
#include "ui/plugin_helpers.h"
#include "ui/menu_engine.h"
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "../gui_config/registries.h"

namespace tmoe::domain {
    RemoteDesktopManager::RemoteDesktopManager(const TmoeConfig &cfg, VncManager &vnc_manager,
                                               DesktopManager &desktop_manager)
        : cfg_(cfg), vnc_manager_(vnc_manager), desktop_manager_(desktop_manager) {
    }

    std::string RemoteDesktopManager::sudo_cmd() const {
        // proot/Termux/已是 root → 不需要 sudo
        if (cfg_.is_termux || cfg_.is_root) return "";
        return "sudo ";
    }

    bool RemoteDesktopManager::install_packages(const std::vector<std::string> &packages) const {
        return SystemHelper::install_packages(packages, cfg_.install_command);
    }

    bool RemoteDesktopManager::install_novnc() {
        Logger::step(_("gui.novnc.installing"));

        // 检查是否已安装（以 /usr/share/novnc 目录为准）
        // 不用 Executor::has("websockify") — 二进制可能在 python3-websockify 包里，
        // apt remove novnc websockify 删不掉它（包名不匹配）
        if (fs::exists("/usr/share/novnc")) {
            Logger::ok(_("gui.novnc.already_installed"));
            return true;
        }

        // 安装依赖
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        PackageManager::install({"novnc", "websockify", "python3-numpy", "python3-websockify"}, family);

        // 如果 apt 源没有 novnc，从 git 安装
        if (!fs::exists("/usr/share/novnc")) {
            Logger::info(_("gui.novnc.cloning"));
            Executor::passthrough("git clone --depth=1 https://github.com/novnc/noVNC.git "
                "/opt/novnc 2>/dev/null || true");
            if (fs::exists("/opt/novnc")) {
                // 创建符号链接
                CommandBuilder("sudo").add_arg("ln").add_flag("-sf").add_arg("/opt/novnc").add_arg("/usr/share/novnc").
                        add_raw(
                            "2>/dev/null || true").execute();
            }
        }

        if (fs::exists("/usr/share/novnc") || fs::exists("/opt/novnc")) {
            // 部署 /usr/local/bin/novnc wrapper（Bash 原版从 install_novnc 拷贝）
            std::string novnc_dir = fs::exists("/usr/share/novnc") ? "/usr/share/novnc" : "/opt/novnc";
            std::string wrapper = "#!/bin/bash\n# tmoe-linux novnc — 自包含启动脚本，对齐 Bash 原版\n"
                "NOVNC_DIR=\"" + novnc_dir + "\"\n"
                "PORT=\"${1:-36080}\"\n"
                "VNC_BACKEND=\"${2:-5902}\"\n"
                "\n# ── 自动启动 VNC（如果未运行）──\n"
                "VNC_DISPLAY=$((VNC_BACKEND - 5900))\n"
                "if ! pgrep -f \"X(vnc|tigervnc|tightvnc).*:${VNC_DISPLAY}\" >/dev/null 2>&1; then\n"
                "  echo \">> VNC not running on :${VNC_DISPLAY}, starting...\"\n"
                "  [ -x /usr/local/bin/startvnc ] && /usr/local/bin/startvnc 2>/dev/null &\n"
                "  sleep 2\n"
                "fi\n"
                "\n# ── 显示局域网地址 ──\n"
                "echo \"noVNC: http://localhost:$PORT/vnc.html\"\n"
                "ip -4 -br a 2>/dev/null | awk '{print $NF}' | grep -v '127.0.0.1' | while read ip; do\n"
                "  echo \"  LAN:  http://${ip%/*}:$PORT/vnc.html\"\n"
                "done\n"
                "\n# ── 启动 websockify ──\n"
                "websockify --web=\"$NOVNC_DIR\" \"$PORT\" localhost:\"$VNC_BACKEND\" &\n"
                "sleep 1\n"
                "\n# ── WSL 自动打开浏览器 ──\n"
                "if grep -qi microsoft /proc/version 2>/dev/null; then\n"
                "  /mnt/c/WINDOWS/system32/cmd.exe /c \"start http://localhost:$PORT/vnc.html\" 2>/dev/null &\n"
                "fi\n";
            SystemHelper::write_file("/usr/local/bin/novnc", wrapper);
            CommandBuilder("sudo").add_arg("chmod").add_arg("+x").add_arg("/usr/local/bin/novnc")
                .add_raw("2>/dev/null || true").execute();
            CommandBuilder("sudo").add_arg("ln").add_flag("-sf").add_arg("novnc")
                .add_arg("/usr/local/bin/startnovnc").add_raw("2>/dev/null || true").execute();
            Logger::ok(_("gui.novnc.install_ok"));
            return true;
        }

        Logger::error(_("gui.novnc.install_failed"));
        return false;
    }


    bool RemoteDesktopManager::configure_novnc() {
        Logger::step(_("gui.novnc.config"));

        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            std::string(_("gui.novnc_port_title")),
            std::string(_("gui.novnc_port_prompt")),
            "novnc_port");

        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.novnc_port_36080")), "1",
            [this](MenuContext&) -> bool {
                novnc_port_ = 36080;
                Logger::info(_f("gui.novnc.port_set", std::to_string(novnc_port_)));
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.novnc_port_36081")), "2",
            [this](MenuContext&) -> bool {
                novnc_port_ = 36081;
                Logger::info(_f("gui.novnc.port_set", std::to_string(novnc_port_)));
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.novnc_port_6080")), "3",
            [this](MenuContext&) -> bool {
                novnc_port_ = 6080;
                Logger::info(_f("gui.novnc.port_set", std::to_string(novnc_port_)));
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            std::string(_("gui.novnc_port_custom")), "4",
            [this](MenuContext&) -> bool {
                std::string input_cmd = cfg_.tui_bin +
                    " --title \"" + std::string(_("gui.novnc_custom_port_title")) +
                    "\" --inputbox \"" + std::string(_("gui.novnc_custom_port_input")) +
                    "\" 10 40 \"36080\"";
                std::string port_str = Executor::tui_select(input_cmd);
                try { novnc_port_ = std::stoi(port_str); } catch (...) { novnc_port_ = 36080; }
                Logger::info(_f("gui.novnc.port_set", std::to_string(novnc_port_)));
                return true;
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
        return true;
    }


    bool RemoteDesktopManager::start_novnc(int port) {
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
        CommandBuilder("sleep").add_arg("2").execute();

        Logger::ok(_f("gui.novnc.url", get_novnc_url()));
        Logger::info(_f("gui.novnc.vnc_backend_port", std::to_string(vnc_manager_.config().rfb_port)));
        // Bash: 显示局域网地址
        std::string lan_ips = vnc_manager_.get_local_ip_addresses();
        if (!lan_ips.empty()) Logger::info(lan_ips);
        return true;
    }


    std::string RemoteDesktopManager::get_novnc_url() const {
        return "http://localhost:" + std::to_string(novnc_port_) + "/vnc.html";
    }

    // ═══════════════════════════════════════════════════════════════
    // XRDP
    // ═══════════════════════════════════════════════════════════════


    bool RemoteDesktopManager::install_xrdp() {
        Logger::step(_("gui.xrdp.installing"));

        std::vector<std::string> pkgs = {"xrdp", "xorgxrdp"};
        if (!install_packages(pkgs)) {
            Logger::error(_("gui.xrdp.install_failed"));
            return false;
        }

        // 修复 xrdp 证书权限: key.pem 默认 640 root:ssl-cert，
        // xrdp 用户必须在 ssl-cert 组中才能读取，否则报 Permission denied
        Executor::shell(
            std::string("if getent group ssl-cert >/dev/null 2>&1; then ")
            + sudo_cmd() + "usermod -a -G ssl-cert xrdp 2>/dev/null || true; "
            + "fi; "
            // 确保 /etc/xrdp 目录权限正确
            + sudo_cmd() + "chown -R root:ssl-cert /etc/xrdp/ 2>/dev/null || true; "
            + sudo_cmd() + "chmod 640 /etc/xrdp/key.pem 2>/dev/null || true; "
            + sudo_cmd() + "chmod 644 /etc/xrdp/cert.pem 2>/dev/null || true");
        // 旧 Bash: chroot/proot 下 xrdp 需要 aid_inet 组才能联网
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            CommandBuilder("sudo").add_arg("usermod").add_flag("-a").add_flag("-G").add_arg("aid_inet").add_arg("xrdp").
                    add_raw(
                        "2>/dev/null || true").execute();
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

        // 写入 WSL 兼容的 xrdp.ini
        set_xrdp_port(3389);

        // 启动（session 配置由调用方通过 configure_xrdp_desktop 完成，Bash 原版安装时不自动配置桌面）
        Executor::passthrough(sudo_cmd() + "service xrdp start 2>/dev/null || " + sudo_cmd() + "systemctl start xrdp 2>/dev/null || true");

        Logger::ok(_("gui.xrdp.install_ok"));
        return true;
    }


    bool RemoteDesktopManager::configure_xrdp_session(std::string_view desktop) {
        // 对应旧 Bash xrdp_onekey (5053-5120) + configure_xrdp_remote_desktop_session (4998-5023)
        // 不覆盖发行版自带的 startwm.sh，而是在其基础上修改
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg("/etc/xrdp").add_raw("2>/dev/null").execute();

        // 如果发行版未提供 startwm.sh，生成包含标准环境初始化的模板.
        // 必须保留 /etc/profile 等加载逻辑，否则 PATH/LANG 等变量缺失。
        if (!fs::exists("/etc/xrdp/startwm.sh")) {
            Logger::debug("11111111111111111111111111111111111");
            SystemHelper::write_file("/etc/xrdp/startwm.sh",
                                     "#!/bin/sh\n"
                                     "# tmoe-linux xrdp startwm\n\n"
                                     "if test -r /etc/profile; then\n"
                                     "    . /etc/profile\n"
                                     "fi\n\n"
                                     "test -x /etc/X11/Xsession && exec /etc/X11/Xsession\n"
                                     "exec /etc/X11/xinit/Xsession\n");
            CommandBuilder("sudo").add_arg("chmod").add_arg("+x").add_arg("/etc/xrdp/startwm.sh").add_raw(
                        "2>/dev/null || true").
                    execute();
        }

        // 注入 WSL/WSLg/GPU 环境修复 (只在尚不存在时插入，避免重复追加)
        // 原生 C++ 替代 shell 的 grep+sed 链: 读取文件一次，检查并注入所有缺失行
        {
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                auto nl_pos = content.find('\n');
                std::string header = (nl_pos != std::string::npos)
                                         ? content.substr(0, nl_pos + 1)
                                         : content + "\n";
                std::string body = (nl_pos != std::string::npos)
                                       ? content.substr(nl_pos + 1)
                                       : "";

                std::string to_inject;
                if (content.find("unset WAYLAND_DISPLAY") == std::string::npos)
                    to_inject += "unset WAYLAND_DISPLAY\n";
                if (content.find("unset XDG_RUNTIME_DIR") == std::string::npos)
                    to_inject += "unset XDG_RUNTIME_DIR\n";
                if (content.find("LIBGL_ALWAYS_SOFTWARE") == std::string::npos)
                    to_inject += "export LIBGL_ALWAYS_SOFTWARE=1\n";
                if (content.find("GALLIUM_DRIVER") == std::string::npos)
                    to_inject += "export GALLIUM_DRIVER=llvmpipe\n";
                if (content.find("export PULSE_SERVER") == std::string::npos)
                    to_inject += "export PULSE_SERVER=127.0.0.1\n";

                if (!to_inject.empty()) {
                    SystemHelper::write_file("/etc/xrdp/startwm.sh", header + to_inject + body);
                }
            }
        }

        // xrdp_onekey 风格: 替换默认 Xsession 路径为 xinit 版本 (C++ 原生替代 sed)
        {
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                // 替换 "exec /etc/X11/Xsession" -> "exec /etc/X11/xinit/Xsession"
                for (auto pos = content.find("exec /etc/X11/Xsession"); pos != std::string::npos;
                     pos = content.find("exec /etc/X11/Xsession", pos)) {
                    // 避免替换已经正确的路径
                    if (content.compare(pos, 31, "exec /etc/X11/xinit/Xsession") != 0) {
                        content.replace(pos, 24, "exec /etc/X11/xinit/Xsession");
                    } else {
                        pos += 31;
                    }
                }
                // 替换 "exec /bin/sh /etc/X11/Xsession" -> "exec /etc/X11/xinit/Xsession"
                for (auto pos = content.find("exec /bin/sh /etc/X11/Xsession");
                     pos != std::string::npos;
                     pos = content.find("exec /bin/sh /etc/X11/Xsession", pos + 1)) {
                    content.replace(pos, 33, "exec /etc/X11/xinit/Xsession");
                }
                SystemHelper::write_file("/etc/xrdp/startwm.sh", content);
            }
        }

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


    bool RemoteDesktopManager::start_xrdp() {
        Logger::step(_("gui.xrdp.starting"));
        if (Executor::passthrough(sudo_cmd() + "service xrdp start 2>/dev/null || " + sudo_cmd() + "systemctl start xrdp 2>/dev/null").
            ok()) {
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


    bool RemoteDesktopManager::stop_xrdp() {
        Logger::step(_("gui.xrdp.stopping"));
        Executor::passthrough(sudo_cmd() + "service xrdp stop 2>/dev/null || " + sudo_cmd() + "systemctl stop xrdp 2>/dev/null || "
            "pkill xrdp 2>/dev/null || true");
        Logger::ok(_("gui.xrdp.stopped"));
        return true;
    }


    bool RemoteDesktopManager::restart_xrdp() {
        stop_xrdp();
        CommandBuilder("sleep").add_arg("1").execute();
        bool ok = start_xrdp();
        if (ok) {
            Logger::info(_("gui.xrdp.connection_info"));
            Logger::info(_("gui.xrdp.wsl_info"));
        }
        return ok;
    }


    bool RemoteDesktopManager::set_xrdp_port(int port) {
        // xrdp 守护进程以 xrdp 用户运行，必须确保它能读取 /etc/xrdp/
        Executor::shell(
            std::string(sudo_cmd() + "mkdir -p /etc/xrdp 2>/dev/null; ")
            + "id xrdp >/dev/null 2>&1 || " + sudo_cmd() + "useradd -r -s /usr/sbin/nologin xrdp 2>/dev/null || true; "
            + sudo_cmd() + "chmod 755 /etc/xrdp 2>/dev/null || true");

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
        CommandBuilder("sudo").add_arg("chmod").add_arg("644").add_arg("/etc/xrdp/xrdp.ini").add_raw(
            "2>/dev/null || true").execute();

        // 验证 xrdp 用户能否读取配置
        auto readable = Executor::shell(
            "sudo -u xrdp cat /etc/xrdp/xrdp.ini >/dev/null 2>&1 && echo 'ok' || "
            "cat /etc/xrdp/xrdp.ini >/dev/null 2>&1 && echo 'ok_root' || echo 'fail'");
        if (readable.stdout_data.find("ok") == std::string::npos) {
            Logger::warn(_("gui.xrdp.permission_warn"));
            Logger::debug("可读性检测: " + readable.stdout_data);
        }

        Logger::ok(_f("gui.xrdp.port_changed", port_str));
        Logger::info(_("gui.xrdp.restart_needed"));
        return true;
    }


    bool RemoteDesktopManager::remove_xrdp() {
        Logger::step(_("gui.xrdp.removing"));
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        PackageManager::remove("xrdp", family);
        PackageManager::remove("xorgxrdp", family);
        Logger::ok(_("gui.xrdp.removed"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // XSDL / VcXsrv / WSL
    // ═══════════════════════════════════════════════════════════════


    void RemoteDesktopManager::run_x11vnc_config_menu() {
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            "CONFIGURE x11vnc",
            "Type startx11vnc to start vncserver",
            "x11vnc_config");

        menu->add_child(LambdaAction::make(
            "pulse_server", "1",
            [this] { vnc_manager_.modify_x11vnc_pulse_server(); Logger::press_enter(); }));
        menu->add_child(LambdaAction::make(
            "resolution", "2",
            [this] { vnc_manager_.modify_x11vnc_resolution(); Logger::press_enter(); }));
        menu->add_child(LambdaAction::make(
            "port", "3",
            [this] { vnc_manager_.modify_x11vnc_port(); Logger::press_enter(); }));
        menu->add_child(LambdaAction::make(
            "edit startx11vnc", "4",
            [] {
                Executor::passthrough(
                    "${EDITOR:-nano} /usr/local/bin/startx11vnc 2>/dev/null || nano /usr/local/bin/startx11vnc");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "remove", "5",
            [this] {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 x11vnc？\" 0 0");
                if (r.exit_code == 0) remove_x11vnc_ext();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "readme", "6",
            [] {
                Logger::info(_("gui.x11vnc.process_management"));
                Logger::info(_("gui.x11vnc.process_start"));
                Logger::info(_("gui.x11vnc.process_stop"));
                Logger::info(_("gui.x11vnc.process_view"));
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "password", "7",
            [] {
                Executor::passthrough("x11vncpasswd 2>/dev/null || x11vnc -storepasswd");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "read doc", "8",
            [] {
                Logger::info(_("gui.x11vnc.documentation"));
                Logger::info(_("gui.x11vnc.doc_start"));
                Logger::info(_("gui.x11vnc.doc_stop"));
                Logger::info(_("gui.x11vnc.doc_connect"));
                Logger::info(_("gui.x11vnc.doc_xvfb"));
                Logger::info(_("gui.x11vnc.doc_man"));
                Logger::press_enter();
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }


    void RemoteDesktopManager::run_novnc_config_menu() {
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            "CONFIGURE NOVNC",
            "Type novnc to start novnc",
            "novnc_config");

        menu->add_child(LambdaAction::make(
            "port", "1",
            [this] {
                std::string port_cmd = cfg_.tui_bin +
                    " --title \"请输入端口\""
                    " --inputbox \"Please type the novnc port, the default is 36080\" 10 50 \"36080\"";
                std::string port = Executor::tui_select(port_cmd);
                if (!port.empty()) {
                    auto content = SystemHelper::read_file("/usr/local/bin/novnc");
                    if (!content.empty()) {
                        auto start = content.find("PORT=\"$");
                        if (start == std::string::npos) start = content.find("PORT=");
                        if (start != std::string::npos) {
                            auto end = content.find_first_of("\n\"", start);
                            content.replace(start, (end != std::string::npos ? end - start : content.size() - start),
                                            "PORT=\"" + port + "\"");
                        }
                        SystemHelper::write_file("/usr/local/bin/novnc", content);
                    }
                }
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "edit startnovnc", "2",
            [] {
                Executor::passthrough("${EDITOR:-nano} /usr/local/bin/novnc 2>/dev/null || nano /usr/local/bin/novnc");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "remove", "3",
            [this] {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 noVNC？\" 0 0");
                if (r.exit_code == 0) remove_novnc();
                Logger::press_enter();
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }


    void RemoteDesktopManager::run_xrdp_menu() {
        // 对应旧 Bash modify_xrdp_conf: 先 proot检查 + Start/Configure yesno
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            Logger::warn(_("gui.xrdp.proot_warn"));
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
                Logger::warn(_("gui.xrdp.not_found_init"));
                install_xrdp();
                configure_xrdp_desktop();
            }
            Executor::passthrough(sudo_cmd() + "service xrdp restart 2>/dev/null || "
                + sudo_cmd() + "systemctl restart xrdp 2>/dev/null || true");
            if (is_running) Logger::ok(_("gui.xrdp.restarted"));
            else Logger::ok(_("gui.xrdp.started_msg"));
            Logger::press_enter();
            return;
        }

        // Configure: 进入 12 项子菜单
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            "CONFIGURE XRDP",
            "Type sudo service xrdp start to start it",
            "xrdp_config");

        menu->add_child(LambdaAction::make(
            "One-key conf", "1",
            [this] {
                Logger::step(_("gui.xrdp.onekey_config"));
                Executor::passthrough(
                    sudo_cmd() + "service xrdp stop 2>/dev/null || " + sudo_cmd() + "systemctl stop xrdp 2>/dev/null || true");
                install_xrdp();
                configure_xrdp_desktop();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "select DE for xrdp", "2",
            [this] { configure_xrdp_desktop(); Logger::press_enter(); }));
        menu->add_child(LambdaAction::make(
            "xrdp port", "3",
            [this] {
                std::string cmd = cfg_.tui_bin +
                    " --title \"xrdp port\""
                    " --inputbox \"请输入 xrdp 端口 (默认 3389)\" 10 40 \"3389\"";
                std::string val = Executor::tui_select(cmd);
                Logger::debug(val);
                if (!val.empty()) {
                    try { set_xrdp_port(std::stoi(val)); } catch (...) {}
                }
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "xrdp.ini", "4",
            [] {
                CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg("/etc/xrdp").add_raw(
                    "2>/dev/null || true").execute();
                Executor::passthrough("${EDITOR:-nano} /etc/xrdp/xrdp.ini 2>/dev/null || nano /etc/xrdp/xrdp.ini");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "startwm.sh", "5",
            [] {
                CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg("/etc/xrdp").add_raw(
                    "2>/dev/null || true").execute();
                Executor::passthrough("${EDITOR:-nano} /etc/xrdp/startwm.sh 2>/dev/null || nano /etc/xrdp/startwm.sh");
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "stop", "6",
            [this] { stop_xrdp(); Logger::press_enter(); }));
        menu->add_child(LambdaAction::make(
            "status", "7",
            [] {
                auto r = Executor::shell("ps aux | grep xrdp | grep -v grep 2>/dev/null");
                if (!r.stdout_data.empty()) {
                    Logger::info(_("gui.xrdp.process_running"));
                    Logger::info(r.stdout_data);
                } else {
                    Logger::info(_("gui.xrdp.process_not_running"));
                }
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "pulse_server", "8",
            [this] {
                std::string cmd = cfg_.tui_bin +
                    " --title \"PULSE SERVER\""
                    " --inputbox \"请输入 PulseAudio 服务器地址\" 10 50 \"127.0.0.1\"";
                std::string val = Executor::tui_select(cmd);
                if (!val.empty()) {
                    auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
                    if (!content.empty()) {
                        auto start = content.find("PULSE_SERVER=");
                        if (start != std::string::npos) {
                            auto end = content.find('\n', start);
                            content.replace(start, (end != std::string::npos ? end - start : content.size() - start),
                                            "PULSE_SERVER=" + val);
                        }
                        SystemHelper::write_file("/etc/xrdp/startwm.sh", content);
                    }
                }
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "reset", "9",
            [this] {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认重置 xrdp 配置？\" 0 0");
                if (r.exit_code == 0) {
                    Logger::step(_("gui.xrdp.reset"));
                    Executor::passthrough(
                        sudo_cmd() + "service xrdp stop 2>/dev/null || " + sudo_cmd() + "systemctl stop xrdp 2>/dev/null || true");
                    Executor::passthrough(sudo_cmd() + "rm -rf /etc/xrdp/xrdp.ini /etc/xrdp/startwm.sh 2>/dev/null || true");
                    install_xrdp();
                    configure_xrdp_desktop();
                }
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "remove", "10",
            [this] {
                auto r = Executor::passthrough(cfg_.tui_bin + " --yesno \"确认卸载 xrdp？\" 0 0");
                if (r.exit_code == 0) remove_xrdp();
                Logger::press_enter();
            }));
        menu->add_child(LambdaAction::make(
            "readme", "11",
            [] {
                Logger::info(_("gui.xrdp.process_management"));
                Logger::info(_("gui.xrdp.management_start"));
                Logger::info(_("gui.xrdp.management_stop"));
                Logger::info(_("gui.xrdp.management_restart"));
                Logger::info(_("gui.xrdp.management_status"));
                Logger::press_enter();
            }));

        add_sandwich_nav(menu);
        MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(ctx).run(menu);
    }


    void RemoteDesktopManager::configure_remote_desktop_environment(std::string_view context) {
        using namespace tmoe::ui;
        auto menu = make_plugin_menu(
            "REMOTE_DESKTOP",
            "Select desktop environment to configure",
            "remote_de_select");

        std::string ctx(context); // 捕获一份可用的 string

        auto make_de_action = [this, &ctx](const std::string& label, const std::string& tag,
                                           const std::string& s1, const std::string& s2) {
            return std::make_shared<LambdaAction>(
                label, tag,
                [this, s1, s2, ctx](MenuContext&) -> bool {
                    std::string remote_session = Executor::has(s1) ? s1 : s2;
                    Logger::info(_f("gui.remote.session_info", remote_session));
                    if (ctx == "xrdp") {
                        configure_xrdp_remote_desktop_session(remote_session);
                        xrdp_restart();
                    } else {
                        configure_x11vnc_remote_desktop_session();
                    }
                    return true;
                });
        };

        menu->add_child(make_de_action("auto", "1", "/etc/X11/xinit/Xsession", "/etc/X11/xinit/Xsession"));
        menu->add_child(make_de_action("xfce", "2", "xfce4-session", "startxfce4"));
        menu->add_child(make_de_action("lxde", "3", "lxsession", "startlxde"));
        menu->add_child(make_de_action("mate", "4", "mate-session", "mate-panel"));
        menu->add_child(make_de_action("lxqt", "5", "startlxqt", "lxqt-session"));
        menu->add_child(make_de_action("kde plasma 5", "6", "startplasma-x11", "startkde"));
        menu->add_child(make_de_action("gnome 3", "7", "gnome-session", "gnome-panel"));
        menu->add_child(make_de_action("cinnamon", "8", "cinnamon-session", "cinnamon-launcher"));
        menu->add_child(make_de_action("dde", "9", "startdde", "dde-launcher"));

        add_sandwich_nav(menu);
        MenuContext mctx{const_cast<TmoeConfig&>(cfg_)};
        MenuEngine(mctx).run(menu);
    }


    void RemoteDesktopManager::configure_xrdp_desktop() {
        // 对应旧 Bash xrdp_desktop_environment → configure_remote_desktop_environment
        configure_remote_desktop_environment("xrdp");
    }


    void RemoteDesktopManager::configure_x11vnc_remote_desktop_session() {
        // 对应旧 Bash configure_x11vnc_remote_desktop_session (gui:1194-1213)
        Logger::step(_("gui.configuring_x11vnc_session"));

        // 1. 确保 startx11vnc thin wrapper 存在 (由 deploy_startup_scripts 生成)
        if (!fs::exists("/usr/local/bin/startx11vnc")) {
            vnc_manager_.deploy_startup_scripts();
        }

        // 2. 原生生成 x11vncpasswd 辅助脚本 (替代旧版 old-version 拷贝)
        SystemHelper::write_file("/usr/local/bin/x11vncpasswd",
                                 "#!/bin/bash\n"
                                 "# tmoe-linux x11vncpasswd — native password helper\n"
                                 "VNC_HOME=\"" + vnc_manager_.config().vnc_home_dir.string() + "\"\n"
                                 "mkdir -p \"$VNC_HOME\" 2>/dev/null\n"
                                 "echo \"Setting x11vnc password...\"\n"
                                 "x11vnc -storepasswd \"$VNC_HOME/x11passwd\" 2>/dev/null || "
                                 "x11vnc -storepasswd ~/.vnc/x11passwd 2>/dev/null || true\n"
                                 "echo 'x11vnc password configured'\n");
        CommandBuilder("sudo").add_arg("chmod").add_arg("a+rx").add_arg("/usr/local/bin/x11vncpasswd").add_raw(
                    "2>/dev/null || true").
                execute();

        // 3. 确保 x11passwd 存在
        if (fs::exists(vnc_manager_.config().passwd_file)) {
            Executor::shell("cd " + vnc_manager_.config().vnc_home_dir.string() + " && "
                            "cp -pvf passwd x11passwd 2>/dev/null || "
                            "cd /usr/local/bin && ./x11vncpasswd 2>/dev/null || true");
        } else {
            Executor::shell("cd /usr/local/bin && ./x11vncpasswd 2>/dev/null || "
                            "x11vnc -storepasswd " +
                            (vnc_manager_.config().vnc_home_dir / "x11passwd").string() + " 2>/dev/null || true");
        }

        Logger::info(_("gui.x11vnc.config_complete"));
        Logger::info(_("gui.x11vnc.restart_stop_hint"));
        Logger::info(std::string(_("gui.switch_to_startvnc")));
    }


    void RemoteDesktopManager::configure_xrdp_remote_desktop_session(const std::string &session_cmd) {
        // 对应旧 Bash configure_xrdp_remote_desktop_session (gui:4998-5023)
        Logger::step(std::string(_("gui.configuring_xrdp_session")) + " " + session_cmd);

        CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg("/etc/xrdp").add_raw("2>/dev/null").execute();
        // 原生 C++ 替代 sed+grep 链: 删除 Xsession 行并修剪尾部空行
        {
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                // 删除包含 "Xsession" 的行
                std::istringstream iss(content);
                std::string line, result;
                while (std::getline(iss, line)) {
                    if (line.find("Xsession") == std::string::npos) {
                        result += line + "\n";
                    }
                }
                // 如果剩余内容包含 "exec"，删除最后两行
                if (result.find("exec") != std::string::npos) {
                    // 去除尾部连续的两个换行分隔行
                    auto trim_last_lines = [](std::string &s, int n) {
                        while (n-- > 0) {
                            if (s.empty()) break;
                            if (s.back() == '\n') s.pop_back(); // 去掉尾部换行
                            auto pos = s.rfind('\n');
                            s.erase(pos == std::string::npos ? 0 : pos + 1);
                        }
                    };
                    trim_last_lines(result, 2);
                    if (!result.empty() && result.back() != '\n') result += '\n';
                }
                SystemHelper::write_file("/etc/xrdp/startwm.sh", result);
            }
        }

        // 添加 Xsession 行
        SystemHelper::append_file("/etc/xrdp/startwm.sh",
                                  "test -x /etc/X11/Xsession && exec /etc/X11/Xsession\n"
                                  "exec /etc/X11/xinit/Xsession\n");

        // 替换为 dbus-launch + 实际会话 (proot 下跳过 --exit-with-session)
        bool is_proot = cfg_.is_termux || cfg_.linux_distro == "Android";
        // 原生 C++ 替代 shell sed 链: 处理 --exit-with-session 和 Xsession 替换
        {
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                if (is_proot) {
                    // 清理已存在的 --exit-with-session 避免级联残留
                    for (auto pos = content.find("--exit-with-session");
                         pos != std::string::npos;
                         pos = content.find("--exit-with-session", pos)) {
                        content.erase(pos, 19);
                    }
                }

                // 定义常量靶标，彻底消除越界风险
                const std::string target_str = "exec /etc/X11/Xsession";
                std::string replacement = "exec dbus-launch" +
                                          std::string(is_proot ? "" : " --exit-with-session") + " " + session_cmd;

                // 使用 target_str.length() 替代魔术数字 24
                for (auto pos = content.find(target_str);
                     pos != std::string::npos;
                     pos = content.find(target_str, pos + replacement.length())) {
                    content.replace(pos, target_str.length(), replacement);
                }

                SystemHelper::write_file("/etc/xrdp/startwm.sh", content);
            }
        }

        Logger::info(std::string(_("gui.xrdp_session_config")));
        // 原生 C++ 替代 cat: 使用 SystemHelper::read_file 读取并显示
        Logger::info(SystemHelper::read_file("/etc/xrdp/startwm.sh"));

        // 不在此处重启: 让调用方控制重启时机，避免 xrdp.ini 未就绪时提前启动
        check_xrdp_status();
        Logger::ok(std::string(_("gui.xrdp_session_done")));
    }

    bool RemoteDesktopManager::stop_novnc() {
        Logger::step(_("gui.novnc.stopping"));
        CommandBuilder("pkill").add_flag("-f").add_arg("websockify").add_raw("2>/dev/null || true").execute();
        Logger::ok(_("gui.novnc.stopped"));
        return true;
    }


    bool RemoteDesktopManager::remove_novnc() {
        Logger::step(_("gui.novnc.removing"));
        stop_novnc();
        // 清理目录和 wrapper 脚本
        CommandBuilder("sudo").add_arg("rm").add_flag("-rf").add_arg("/opt/novnc").add_arg("/usr/share/novnc")
            .add_arg("/usr/local/bin/novnc").add_arg("/usr/local/bin/startnovnc").add_raw(
            "2>/dev/null || true").execute();
        // apt 装的包（和 install_novnc 对应：novnc, python3-websockify, python3-numpy）
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        PackageManager::remove("novnc", family);
        PackageManager::remove("websockify", family);
        PackageManager::remove("python3-websockify", family);
        PackageManager::remove("python3-numpy", family);
        Logger::ok(_("gui.novnc.removed"));
        return true;
    }


    void RemoteDesktopManager::x11vnc_onekey() {
        x11vnc_warning();
        // configure_remote_desktop_environment for x11vnc
        configure_remote_desktop_environment("x11vnc");
    }


    void RemoteDesktopManager::xrdp_onekey() {
        Logger::step(_("gui.xrdp.onekey_config"));
        // 安装 xrdp (按发行版)
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        if (!Executor::has("xrdp-keygen") && !fs::exists("/usr/sbin/xrdp")) {
            if (family == DistroFamily::Gentoo) {
                Executor::passthrough("sudo emerge -avk layman 2>/dev/null; sudo layman -a bleeding-edge 2>/dev/null; "
                    "layman -S 2>/dev/null || true");
            }
            PackageManager::install("xrdp", family);
        }
        // 修复 xrdp 证书权限: key.pem 默认 640 root:ssl-cert
        Executor::shell(
            std::string("if getent group ssl-cert >/dev/null 2>&1; then ")
            + sudo_cmd() + "usermod -a -G ssl-cert xrdp 2>/dev/null || true; "
            + "fi; "
            + sudo_cmd() + "chown -R root:ssl-cert /etc/xrdp/ 2>/dev/null || true; "
            + sudo_cmd() + "chmod 640 /etc/xrdp/key.pem 2>/dev/null || true; "
            + sudo_cmd() + "chmod 644 /etc/xrdp/cert.pem 2>/dev/null || true");
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            CommandBuilder("sudo").add_arg("usermod").add_flag("-a").add_flag("-G").add_arg("aid_inet").add_arg("xrdp").
                    add_raw(
                        "2>/dev/null || true").execute();
        }
        // polkit 规则
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-pv").add_arg("/etc/polkit-1/localauthority.conf.d").add_arg(
            "/etc/polkit-1/localauthority/50-local.d/").execute();
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
        // 修复 startwm.sh — 注入 WSL/WSLg/GPU 环境修复 (幂等插入，C++ 原生替代 sed+grep)
        {
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                // 替换 Xsession 路径为 xinit 版本
                for (auto pos = content.find("exec /etc/X11/Xsession"); pos != std::string::npos;
                     pos = content.find("exec /etc/X11/Xsession", pos)) {
                    if (content.compare(pos, 31, "exec /etc/X11/xinit/Xsession") != 0) {
                        content.replace(pos, 24, "exec /etc/X11/xinit/Xsession");
                    } else {
                        pos += 31;
                    }
                }
                for (auto pos = content.find("exec /bin/sh /etc/X11/Xsession");
                     pos != std::string::npos;
                     pos = content.find("exec /bin/sh /etc/X11/Xsession", pos + 1)) {
                    content.replace(pos, 33, "exec /etc/X11/xinit/Xsession");
                }

                // 注入缺失的环境变量行 (shebang 之后)
                auto nl = content.find('\n');
                std::string hdr = (nl != std::string::npos) ? content.substr(0, nl + 1) : content + "\n";
                std::string bdy = (nl != std::string::npos) ? content.substr(nl + 1) : "";
                std::string inj;
                if (content.find("unset WAYLAND_DISPLAY") == std::string::npos)
                    inj += "unset WAYLAND_DISPLAY\n";
                if (content.find("unset XDG_RUNTIME_DIR") == std::string::npos)
                    inj += "unset XDG_RUNTIME_DIR\n";
                if (content.find("LIBGL_ALWAYS_SOFTWARE") == std::string::npos)
                    inj += "export LIBGL_ALWAYS_SOFTWARE=1\n";
                if (content.find("GALLIUM_DRIVER") == std::string::npos)
                    inj += "export GALLIUM_DRIVER=llvmpipe\n";
                if (content.find("export PULSE_SERVER") == std::string::npos)
                    inj += "export PULSE_SERVER=127.0.0.1\n";

                content = inj.empty() ? (hdr + bdy) : (hdr + inj + bdy);
                SystemHelper::write_file("/etc/xrdp/startwm.sh", content);
            }
        }

        // WSL 处理
        if (cfg_.is_wsl) {
            Logger::info(_("gui.xrdp.wsl_port_setup"));
            Executor::shell(
                "sed -i 's/^export PULSE_SERVER=.*/export PULSE_SERVER=$(ip route list table 0 | head -n 1 | "
                "awk -F '\"'\"'default via '\"'\"' \"'\"'{print $2}'\"'\"' |awk \"'\"'{print $1}'\"'\"')/' "
                "/etc/xrdp/startwm.sh 2>/dev/null || true");
        }
        xrdp_port();
        xrdp_restart();
        Logger::ok(_("gui.xrdp.onekey_done"));
    }


    void RemoteDesktopManager::check_xrdp_status() const {
        if (Executor::has("service")) {
            Executor::passthrough("service xrdp status 2>/dev/null | head -n 24");
        } else {
            Executor::passthrough("systemctl status xrdp 2>/dev/null | head -n 24");
        }
    }


    void RemoteDesktopManager::xrdp_port() {
        // 原生 C++ 替代 cat|grep|cut 管道: 从 xrdp.ini 提取端口号
        std::string current_port; {
            auto content = SystemHelper::read_file("/etc/xrdp/xrdp.ini");
            auto pos = content.find("port=");
            if (pos != std::string::npos) {
                // 跳过 "port=" 前缀，提取到第一个非 URI scheme 部分
                // xrdp.ini 格式: port=tcp://0.0.0.0:3389 或 port=3389
                auto val_start = pos + 5; // strlen("port=")
                auto eq_or_nl = content.find_first_of("=\n", val_start);
                if (eq_or_nl != std::string::npos && content[eq_or_nl] == '=') {
                    // URI 格式: port=tcp://0.0.0.0:PORT，提取冒号后部分
                    auto colon = content.rfind(':', eq_or_nl);
                    if (colon != std::string::npos && colon > eq_or_nl) {
                        auto end = content.find_first_of("\n\r ", colon);
                        current_port = content.substr(colon + 1,
                                                      (end != std::string::npos ? end - colon - 1 : std::string::npos));
                    }
                } else {
                    // 简单格式: port=3389
                    auto end = content.find_first_of("\n\r ", val_start);
                    current_port = content.substr(val_start,
                                                  (end != std::string::npos ? end - val_start : std::string::npos));
                }
            }
        }
        trim_newline(current_port);
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


    void RemoteDesktopManager::xrdp_restart() {
        // 原生 C++ 替代 cat|grep|cut 管道: 从 xrdp.ini 提取端口号
        // 从 xrdp.ini 提取端口号，支持 port=3389 和 port=tcp://0.0.0.0:3389 两种格式
        std::string rdp_port = "3389";
        {
            auto content = SystemHelper::read_file("/etc/xrdp/xrdp.ini");
            auto pos = content.find("port=");
            if (pos != std::string::npos) {
                auto val_start = pos + 5;
                auto end = content.find_first_of("\n\r", val_start);
                std::string raw = content.substr(val_start,
                    (end != std::string::npos ? end - val_start : std::string::npos));
                // 去掉空白
                while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ')) raw.pop_back();
                // port=tcp://0.0.0.0:3389 → 取冒号后的数字
                auto colon = raw.rfind(':');
                if (colon != std::string::npos && colon + 1 < raw.size())
                    rdp_port = raw.substr(colon + 1);
                else
                    rdp_port = raw;
            }
        }
        Executor::passthrough(sudo_cmd() + "service xrdp restart 2>/dev/null || "
            + sudo_cmd() + "systemctl restart xrdp 2>/dev/null || true");
        check_xrdp_status();
        Logger::info(_("gui.xrdp.port_info") + rdp_port);
        Logger::info(_("gui.xrdp.localhost_addr") + rdp_port);
        if (cfg_.is_wsl) {
            Logger::info(_("gui.xrdp.wsl_audio_start"));
            Executor::shell("cd '/mnt/c/Users/Public/Downloads/pulseaudio/bin' 2>/dev/null && "
                "/mnt/c/WINDOWS/system32/cmd.exe /c \"start .\\pulseaudio.bat\" 2>/dev/null &");
        }
    }


    void RemoteDesktopManager::remove_x11vnc_ext() {
        Logger::step(_("gui.x11vnc.stopping_and_removing"));
        vnc_manager_.stop_x11vnc();
        CommandBuilder("sudo").add_arg("rm").add_flag("-rfv").add_arg("/usr/local/bin/startx11vnc").add_raw(
                    "2>/dev/null || true").
                execute();
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        PackageManager::remove("x11vnc", family);
        Logger::ok(_("gui.x11vnc.removed"));
    }


    void RemoteDesktopManager::x11vnc_warning() const {
        Logger::info("------------------------");
        Logger::info(std::string(_("gui.vnc.x11vnc_intro")));
        Logger::info(std::string(_("gui.vnc.x11vnc_details")));
        Logger::info(std::string(_("gui.vnc.recommendation")));
        Logger::info(std::string(_("gui.vnc.performance_no_accel")));
        Logger::info(std::string(_("gui.vnc.performance_accel")));
        Logger::info(std::string(_("gui.vnc.configure_multiple")));
        Logger::info("------------------------");

        // Bash: 自动检测并安装 x11vnc、xvfb 依赖
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        if (!Executor::has("x11vnc"))
            PackageManager::install("x11vnc", family);
        if (!Executor::has("Xvfb"))
            PackageManager::install("xvfb", family);
        if (family == DistroFamily::Debian || family == DistroFamily::Arch)
            PackageManager::install("pavucontrol", family);

        // Bash: 交互模式 → do_you_want_to_continue
        if (!desktop_manager_.is_auto_install_mode())
            Logger::press_enter();
    }


    void RemoteDesktopManager::xrdp_pulse_server() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"MODIFY PULSE SERVER ADDRESS\""
                          " --inputbox \"输入 PulseAudio 服务器地址\\\\n当前为\"$(grep 'PULSE_SERVER' /etc/xrdp/startwm.sh | "
                          "grep -v '^#' | cut -d '=' -f 2 | head -n 1)\"\\\\n例如 127.0.0.1\" 15 50";
        std::string addr = Executor::tui_select(cmd);
        if (!addr.empty()) {
            // 原生 C++ 替代 grep+sed 链: 更新或添加 PULSE_SERVER 行
            auto content = SystemHelper::read_file("/etc/xrdp/startwm.sh");
            if (!content.empty()) {
                bool found = false;
                std::istringstream iss(content);
                std::string line, result;
                while (std::getline(iss, line)) {
                    // 匹配以 "export" 开头且包含 "PULSE_SERVER" 的行
                    if (starts_with(line, "export") && contains(line, "PULSE_SERVER")) {
                        result += "export PULSE_SERVER=" + addr + "\n";
                        found = true;
                    } else {
                        result += line + "\n";
                    }
                }
                // 如果不存在匹配行，在 shebang 后追加
                if (!found) {
                    auto nl = result.find('\n');
                    if (nl != std::string::npos) {
                        result.insert(nl + 1, "export PULSE_SERVER=" + addr + "\n");
                    }
                }
                SystemHelper::write_file("/etc/xrdp/startwm.sh", result);
            }
        }
    }


    void RemoteDesktopManager::x11vnc_doc() const {
        Logger::info("x11vnc 文档: http://www.karlrunge.com/x11vnc/x11vnc_opts.html");
    }


    void RemoteDesktopManager::do_you_want_to_configure_novnc() {
        // 对应旧 Bash do_you_want_to_configure_novnc (gui:5792-5826)
        // Bash: 终端 printf 引导文本 + do_you_want_to_continue (Y/N read)
        vnc_manager_.check_the_which_command();
        Logger::info(_("gui.novnc.startup_hint"));
        Logger::info(_("gui.novnc.config_complete_hint"));
        Logger::info("------------------------");
        Logger::info(_("gui.novnc.configure_prompt"));

        // Bash: 交互模式 → Y/N 终端确认；自动模式 → if_container_is_arm → ARM 跳过
        if (!desktop_manager_.is_auto_install_mode()) {
            if (!Logger::confirm_yes_default(_("gui.novnc.confirm_install"))) return;
        } else {
            // Bash if_container_is_arm: ARM 架构在自动模式下跳过 noVNC（包可能不可用）
            bool is_arm = (cfg_.arch.find("arm") != std::string::npos);
            if (is_arm) {
                Logger::info(_("gui.novnc.arm_skip"));
                return;
            }
        }
        install_novnc();

        // Achievement 解锁
        std::string tmoe_dir = "/usr/local/etc/tmoe-linux";
        if (!fs::exists(tmoe_dir + "/achievement01")) {
            Logger::info(_("gui.novnc.achievement_unlock"));
            Logger::info(_("gui.novnc.achievement_detail"));
            CommandBuilder("mkdir").add_flag("-p").add_arg(tmoe_dir).add_raw("2>/dev/null || true").execute();
            SystemHelper::write_file(tmoe_dir + "/achievement01", "vnc master\n");
        }
        // VNC 命令提示 → 终端输出（Bash 原版也是 printf；交互模式额外有 whiptail msgbox）
        Logger::info(Executor::has("apt-get")
                     ? _("gui.novnc.vnc_commands_with_tiger")
                     : _("gui.novnc.vnc_commands"));
        Logger::info(_("gui.novnc.vnc_master"));

        // Bash: 交互模式最后再弹一个 whiptail msgbox 展示 VNC 命令摘要
        if (!desktop_manager_.is_auto_install_mode()) {
            std::string vnc_msg = Executor::has("apt-get")
                                      ? _("gui.novnc.vnc_commands_with_tiger")
                                      : _("gui.novnc.vnc_commands");
            Executor::passthrough(cfg_.tui_bin + " --title \"VNC COMMANDS\" --msgbox \"" + vnc_msg + "\" 12 55");
        }
    }


    void RemoteDesktopManager::x11vnc_process_readme() const {
        Logger::info(_("gui.x11vnc.readme_start"));
        Logger::info(_("gui.x11vnc.readme_stop"));
        Logger::info(_("gui.x11vnc.readme_audio"));
    }


    void RemoteDesktopManager::xrdp_systemd() const {
        Logger::info(_("gui.xrdp.systemd_management"));
        Logger::info(_("gui.xrdp.systemd_start"));
        Logger::info(_("gui.xrdp.systemd_stop"));
        Logger::info(_("gui.xrdp.systemd_status"));
        Logger::info(_("gui.xrdp.systemd_enable"));
        Logger::info(_("gui.xrdp.systemd_disable"));
        Logger::info(_("gui.xrdp.service_command"));
        Logger::info(_("gui.xrdp.service_start_stop_status"));
        Logger::info(_("gui.xrdp.initd_management"));
        Logger::info(_("gui.xrdp.initd_commands"));
    }


    void RemoteDesktopManager::xrdp_reset() {
        // 对应旧 Bash xrdp_reset (gui:5227-5238): 从备份恢复 xrdp 配置
        Logger::step(_("gui.xrdp.resetting"));

        // 旧 Bash 有 do_you_want_to_continue 确认; 这里用 Logger::confirm
        if (!Logger::confirm(_("gui.xrdp.reset_confirm"))) {
            return;
        }

        Executor::passthrough(sudo_cmd() + "service xrdp stop 2>/dev/null || " + sudo_cmd() + "systemctl stop xrdp 2>/dev/null || "
            "pkill xrdp 2>/dev/null || true");

        CommandBuilder("sudo").add_arg("rm").add_flag("-f").add_arg(
                    "/etc/polkit-1/localauthority/50-local.d/45-allow.colord.pkla")
                .add_arg("/etc/polkit-1/localauthority.conf.d/02-allow-colord.conf").add_raw("2>/dev/null || true").
                execute();

        // 尝试从备份恢复
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string backup_ini = home + "/.config/tmoe-linux/xrdp.ini";
        std::string backup_wm = home + "/.config/tmoe-linux/startwm.sh";

        bool restored = false;
        if (fs::exists(backup_ini) && fs::exists(backup_wm)) {
            // 旧 Bash: cd ~/.config/tmoe-linux && cp -pvf xrdp.ini startwm.sh /etc/xrdp/
            auto cp_result = Executor::shell("sudo cp -pf " + backup_ini + " /etc/xrdp/xrdp.ini 2>/dev/null && "
                                             "sudo cp -pf " + backup_wm + " /etc/xrdp/startwm.sh 2>/dev/null");
            if (cp_result.ok()) {
                restored = true;
                Logger::ok(_("gui.xrdp.restored_from_backup"));
            }
        }

        if (!restored) {
            // 备份不存在，重新生成配置而不是损坏现有文件
            Logger::warn(_("gui.xrdp.no_backup_found"));
            // 先写 xrdp.ini (确保重启前配置已就绪)
            set_xrdp_port(3389);
            // 再生成 startwm.sh
            configure_xrdp_session("xfce");
            Logger::ok(_("gui.xrdp.regenerated"));
        }

        // 重新生成 polkit 规则
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-pv").add_arg("/etc/polkit-1/localauthority.conf.d")
                .add_arg("/etc/polkit-1/localauthority/50-local.d/").add_raw("2>/dev/null").execute();;
        SystemHelper::write_file("/etc/polkit-1/localauthority.conf.d/02-allow-colord.conf",
                                 generate_polkit_colord_conf());
        SystemHelper::write_file("/etc/polkit-1/localauthority/50-local.d/45-allow.colord.pkla",
                                 generate_polkit_colord_pkla());

        xrdp_restart();
    }


    std::string RemoteDesktopManager::generate_polkit_colord_conf() const {
        return gui_config::POLKIT_COLORD_CONF;
    }


    std::string RemoteDesktopManager::generate_polkit_colord_pkla() const {
        return gui_config::POLKIT_COLORD_PKLA;
    }
} // namespace tmoe::domain
