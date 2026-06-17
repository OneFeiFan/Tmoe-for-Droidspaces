#include "gui.h"
#include "core/i18n.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tmoe::domain {
    // ═══════════════════════════════════════════════════════════════
    // VncConfig 实现
    // ═══════════════════════════════════════════════════════════════

    void VncConfig::init_defaults() {
        server = "tiger";
        server_bin = "tigervnc";
        display = 2;
        pixel_depth = 24;
        zlib_level = 0;
        always_shared = true;
        pulse_port = 4713;

        update_port();

        const char *home = std::getenv("HOME");
        fs::path home_dir = home ? fs::path(home) : fs::path("/root");
        vnc_home_dir = home_dir / ".vnc";
        xstartup_file = vnc_home_dir / "xstartup";
        passwd_file = vnc_home_dir / "passwd";
        xsession_file = "/etc/X11/xinit/Xsession";
        tigervnc_config = "/etc/tigervnc/vncserver-config-tmoe";

        // 默认依赖 (Debian 系)
        dep_viewer = "tigervnc-viewer";
        dep_server = "tigervnc-standalone-server";
        dep_extra = "xfonts-100dpi xfonts-75dpi xfonts-scalable";
    }

    void VncConfig::update_port() {
        rfb_port = 5900 + display;
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面环境注册表
    // ═══════════════════════════════════════════════════════════════

    const std::vector<DesktopInfo> &GUIManager::desktop_registry() {
        static const std::vector<DesktopInfo> reg = {
            // rootless DEs (可在 proot 容器中运行)
            {
                "xfce", "Xfce 4", "xfce4-session", "startxfce4", "lightdm", false, "xfce4 xfce4-goodies",
                "最推荐的轻量桌面，兼容性最好"
            },
            {"lxde", "LXDE", "lxsession", "startlxde", "lightdm", false, "lxde lxde-common", "超轻量，适合低配设备"},
            {
                "mate", "MATE", "mate-session", "mate-panel", "lightdm", false, "mate-desktop-environment",
                "GNOME 2 经典延续"
            },
            {"lxqt", "LXQt", "startlxqt", "lxqt-session", "sddm", false, "lxqt lxqt-qtplugin", "基于 Qt 的轻量桌面"},
            // VM/rootful DEs
            {
                "kde", "KDE Plasma 5", "startplasma-x11", "startkde", "sddm", true, "kde-plasma-desktop",
                "全功能，tigervnc 强烈推荐"
            },
            {
                "cinnamon", "Cinnamon", "cinnamon-session", "cinnamon", "lightdm", true, "cinnamon-desktop-environment",
                "类 Windows 体验"
            },
            {
                "gnome", "GNOME 3", "gnome-session", "gnome-shell-x11", "gdm", true, "gnome-session gnome-shell",
                "资源消耗大，tiger 推荐"
            },
            {"dde", "Deepin DDE", "startdde", "dde-launcher", "lightdm", true, "dde dde-desktop", "国产精美桌面"},
            {"ukui", "UKUI", "ukui-session", "ukui-panel", "lightdm", true, "ukui-desktop-environment", "银河麒麟桌面"},
            {"budgie", "Budgie", "budgie-desktop", "budgie-panel", "lightdm", true, "budgie-desktop", "Solus 风格的现代桌面"},
            {
                "cutefish", "Cutefish", "cutefish-session", "cutefish-launcher", "lightdm", true, "cutefish",
                "类 macOS 风格 (较新)"
            },
        };
        return reg;
    }

    /** 50+ 常用窗口管理器注册表。 */
    const std::map<std::string, std::string> &GUIManager::window_manager_registry() {
        static const std::map<std::string, std::string> reg = {
            {"openbox", "openbox obconf obmenu"},
            {"icewm", "icewm icewm-common"},
            {"fluxbox", "fluxbox"},
            {"jwm", "jwm"},
            {"fvwm", "fvwm fvwm-icons"},
            {"i3", "i3 i3status i3lock dmenu"},
            {"i3-gaps", "i3-gaps i3status i3lock dmenu"},
            {"awesome", "awesome"},
            {"xmonad", "xmonad xmobar"},
            {"bspwm", "bspwm sxhkd"},
            {"dwm", "dwm st"},
            {"herbstluftwm", "herbstluftwm"},
            {"qtile", "qtile"},
            {"spectrwm", "spectrwm"},
            {"sway", "sway swaylock swayidle"},
            {"hyprland", "hyprland"},
            {"enlightenment", "enlightenment"},
            {"cwm", "cwm"},
            {"wmii", "wmii"},
            {"ratpoison", "ratpoison"},
            {"stumpwm", "stumpwm"},
            {"blackbox", "blackbox"},
            {"pekwm", "pekwm"},
            {"windowmaker", "wmaker"},
            {"afterstep", "afterstep"},
            {"twm", "twm"},
            {"matchbox", "matchbox-window-manager"},
            {"compiz", "compiz compiz-core"},
            {"mutter", "mutter"},
            {"kwin", "kwin-x11"},
        };
        return reg;
    }

    // ═══════════════════════════════════════════════════════════════
    // 构造 / 析构
    // ═══════════════════════════════════════════════════════════════

    GUIManager::GUIManager(const TmoeConfig &cfg) : cfg_(cfg) {
        vnc_config_.init_defaults();
    }

    // ═══════════════════════════════════════════════════════════════
    // 私有辅助 — 文件 I/O
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::write_file(const fs::path &path, std::string_view content) const {
        try {
            fs::create_directories(path.parent_path());
            std::ofstream ofs(path, std::ios::trunc);
            if (!ofs.is_open()) return false;
            ofs << content;
            return ofs.good();
        } catch (const std::exception &e) {
            Logger::error(std::string("写入文件失败: ") + path.string() + " — " + e.what());
            return false;
        }
    }

    bool GUIManager::append_file(const fs::path &path, std::string_view content) const {
        try {
            fs::create_directories(path.parent_path());
            std::ofstream ofs(path, std::ios::app);
            if (!ofs.is_open()) return false;
            ofs << content;
            return ofs.good();
        } catch (const std::exception &e) {
            Logger::error(std::string("追加文件失败: ") + path.string() + " — " + e.what());
            return false;
        }
    }

    std::string GUIManager::read_file(const fs::path &path) const {
        try {
            std::ifstream ifs(path);
            if (!ifs.is_open()) return {};
            std::ostringstream oss;
            oss << ifs.rdbuf();
            return oss.str();
        } catch (...) {
            return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 私有辅助 — 包安装
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_packages(const std::vector<std::string> &packages) const {
        std::ostringstream oss;
        for (const auto &pkg: packages) oss << pkg << " ";
        std::string cmd = cfg_.install_command + " " + oss.str();
        Logger::step("安装软件包: " + oss.str());
        return Executor::shell(cmd).ok();
    }

    // ═══════════════════════════════════════════════════════════════
    // VNC 服务端安装与配置
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_vnc_server() {
        Logger::step(_("gui.vnc.install"));

        // 安装 tigervnc standalon e server + viewer
        std::vector<std::string> pkgs = {
            "tigervnc-standalone-server", "tigervnc-viewer",
            "xfonts-100dpi", "xfonts-75dpi", "xfonts-scalable",
            "x11vnc", "xvfb"
        };

        if (!install_packages(pkgs)) {
            Logger::warn("部分 VNC 组件安装失败，尝试继续...");
        }

        // 检查 Xvnc 命令
        return check_xvnc_command();
    }

    bool GUIManager::check_xvnc_command() {
        if (Executor::has("Xvnc")) {
            Logger::ok("Xvnc 命令可用");
            return true;
        }
        if (Executor::has("Xtigervnc")) {
            Logger::ok("Xtigervnc 命令可用");
            return true;
        }
        if (Executor::has("Xtightvnc")) {
            Logger::ok("Xtightvnc 命令可用");
            return true;
        }

        Logger::warn("未检测到 VNC 服务端，将自动安装...");
        return install_packages({"tigervnc-standalone-server"});
    }

    bool GUIManager::choose_vnc_server() {
        // TigerVNC vs TightVNC 选择
        // 对应 Bash: which_vnc_server_do_you_prefer()

        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"选择 VNC 服务端\""
                               " --menu \"请选择 VNC 服务端:\\n\\n"
                               "TigerVNC: 兼容性好，支持 TLS 加密\\n"
                               "TightVNC: 压缩效率高，低带宽流畅\\n\\n"
                               "KDE/GNOME/Cinnamon/DDE 推荐 TigerVNC\" 0 0 0 "
                               "\"tiger\" \"TigerVNC (推荐)\" "
                               "\"tight\" \"TightVNC (压缩优秀)\" ";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice.empty()) {
            choice = "tiger";
        }

        if (choice == "tight") {
            vnc_config_.server = "tight";
            vnc_config_.server_bin = "tightvnc";
            vnc_config_.dep_server = "tightvncserver";
            vnc_config_.dep_viewer = "tigervnc-viewer";
            Logger::info("已选择 TightVNC");
        } else {
            vnc_config_.server = "tiger";
            vnc_config_.server_bin = "tigervnc";
            vnc_config_.dep_server = "tigervnc-standalone-server";
            vnc_config_.dep_viewer = "tigervnc-viewer";
            Logger::info("已选择 TigerVNC");
        }

        return true;
    }

    bool GUIManager::configure_vnc_password(std::string_view password) {
        Logger::step(_("gui.vnc.password"));

        // 确保目录存在
        fs::create_directories(vnc_config_.vnc_home_dir);

        std::string passwd_to_use;
        if (!password.empty()) {
            passwd_to_use = std::string(password);
        } else {
            // 交互式输入: 用 whiptail passwordbox
            std::string pass_cmd = cfg_.tui_bin +
                                   " --title \"VNC 密码\" --passwordbox \"请输入 VNC 密码 (6-8 位):\" 10 40";
            auto result = Executor::shell(pass_cmd + " 2>&1");
            passwd_to_use = result.stdout_data;

            // 去除尾部换行
            while (!passwd_to_use.empty() && (passwd_to_use.back() == '\n' || passwd_to_use.back() == '\r'))
                passwd_to_use.pop_back();

            if (passwd_to_use.empty()) {
                Logger::warn("密码为空，将使用无密码模式");
                return true;
            }
        }

        // 写入密码文件 (vncpasswd 需要 stdin 管道)
        std::string cmd = "echo '" + passwd_to_use + "' | vncpasswd -f > " +
                          vnc_config_.passwd_file.string() + " 2>/dev/null";

        if (Executor::shell(cmd).ok()) {
            vnc_config_.password = passwd_to_use;
            // 修复权限 (仅 root)
            if (cfg_.is_root) {
                Executor::shell("chmod 600 " + vnc_config_.passwd_file.string() + " 2>/dev/null");
            }
            Logger::ok("VNC 密码已设置 -> " + vnc_config_.passwd_file.string());
            return true;
        }

        Logger::error("VNC 密码设置失败");
        return false;
    }

    bool GUIManager::configure_vnc_defaults() {
        // 对应 Bash: configure_xvnc()
        Logger::step("配置 TigerVNC 默认参数...");

        fs::create_directories(vnc_config_.tigervnc_config.parent_path());

        std::ostringstream config;
        int w = vnc_config_.resolution_w;
        int h = vnc_config_.resolution_h;
        config << "securitytypes=vncauth,tlsvnc\n"
                << "geometry=" << w << "x" << h << "\n"
                << "desktop=tmoe-linux\n"
                << "depth=" << vnc_config_.pixel_depth << "\n"
                << "ZlibLevel=" << vnc_config_.zlib_level << "\n"
                << "localhost=no\n";

        return write_file(vnc_config_.tigervnc_config, config.str());
    }

    std::string GUIManager::generate_xsession_content(std::string_view desktop) const {
        DesktopInfo info = get_desktop_info(desktop);

        std::ostringstream script;
        script << "#!/bin/bash\n"
                << "# tmoe-linux Xsession — 自动生成，请勿手动修改\n\n"
                << "SESSION_01=\"" << info.session_cmd1 << "\"\n"
                << "SESSION_02=\"" << info.session_cmd2 << "\"\n\n"
                << "# 检测可用桌面会话命令\n"
                << "set_session_env() {\n"
                << "    if command -v \"$SESSION_01\" >/dev/null 2>&1; then\n"
                << "        REMOTE_DESKTOP_SESSION=\"$SESSION_01\"\n"
                << "    elif command -v \"$SESSION_02\" >/dev/null 2>&1; then\n"
                << "        REMOTE_DESKTOP_SESSION=\"$SESSION_02\"\n"
                << "    else\n"
                << "        echo \"错误: 未找到桌面会话命令\"\n"
                << "        exit 1\n"
                << "    fi\n"
                << "}\n\n"
                << "# 自动打开终端\n"
                << "open_terminal() {\n"
                << "    if command -v xfce4-terminal >/dev/null 2>&1; then\n"
                << "        xfce4-terminal &\n"
                << "    elif command -v x-terminal-emulator >/dev/null 2>&1; then\n"
                << "        x-terminal-emulator &\n"
                << "    elif command -v konsole >/dev/null 2>&1; then\n"
                << "        konsole &\n"
                << "    elif command -v gnome-terminal >/dev/null 2>&1; then\n"
                << "        gnome-terminal &\n"
                << "    elif command -v lxterminal >/dev/null 2>&1; then\n"
                << "        lxterminal &\n"
                << "    elif command -v mate-terminal >/dev/null 2>&1; then\n"
                << "        mate-terminal &\n"
                << "    fi\n"
                << "}\n\n"
                << "# 启动桌面会话\n"
                << "start_session() {\n"
                << "    set_session_env\n"
                << "    open_terminal\n"
                << "    echo \"启动桌面会话: $REMOTE_DESKTOP_SESSION\"\n"
                << "    exec dbus-launch --exit-with-session $REMOTE_DESKTOP_SESSION\n"
                << "}\n\n"
                << "start_session\n";

        return script.str();
    }

    std::string GUIManager::generate_xstartup_content() const {
        std::ostringstream script;
        script << "#!/bin/bash\n"
                << "# tmoe-linux VNC xstartup — 链接到 Xsession\n\n"
                << "unset SESSION_MANAGER\n"
                << "unset DBUS_SESSION_BUS_ADDRESS\n\n"
                << "# 修复部分应用需要的环境变量\n"
                << "export XDG_SESSION_TYPE=x11\n"
                << "export DESKTOP_SESSION=tmoe_linux\n\n"
                << "# 启动 fcitx 输入法\n"
                << "if command -v fcitx >/dev/null 2>&1; then\n"
                << "    fcitx-autostart >/dev/null 2>&1 &\n"
                << "fi\n\n"
                << "# 启动 ibus 输入法\n"
                << "if command -v ibus-daemon >/dev/null 2>&1; then\n"
                << "    ibus-daemon -drx >/dev/null 2>&1 &\n"
                << "fi\n\n"
                << "# 执行 Xsession\n"
                << "if [ -x " << vnc_config_.xsession_file.string() << " ]; then\n"
                << "    exec " << vnc_config_.xsession_file.string() << "\n"
                << "else\n"
                << "    xterm -geometry 80x24+10+10 -ls -title \"tmoe VNC Desktop\" &\n"
                << "    exec twm\n"
                << "fi\n";
        return script.str();
    }

    bool GUIManager::configure_xstartup(std::string_view desktop) {
        // 对应 Bash: configure_vnc_xstartup()
        Logger::step("配置 VNC Xsession — 桌面环境: " + std::string(desktop));

        // 1. 创建 Xsession
        std::string xsession_content = generate_xsession_content(desktop);
        if (!write_file(vnc_config_.xsession_file, xsession_content)) {
            Logger::error("写入 Xsession 失败: " + vnc_config_.xsession_file.string());
            return false;
        }
        Executor::shell("chmod +x " + vnc_config_.xsession_file.string());

        // 2. 确保目录和 machine-id
        Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
        if (!fs::exists("/etc/machine-id")) {
            Executor::shell("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
                "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
        }
        if (!fs::exists("/var/lib/dbus/machine-id")) {
            Executor::shell("cp /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
        }

        // 3. 创建 ~/.vnc/xstartup
        std::string xstartup_content = generate_xstartup_content();
        if (!write_file(vnc_config_.xstartup_file, xstartup_content)) {
            Logger::error("写入 xstartup 失败: " + vnc_config_.xstartup_file.string());
            return false;
        }
        Executor::shell("chmod +x " + vnc_config_.xstartup_file.string());

        // 4. 配置 tigervnc 默认配置
        configure_vnc_defaults();

        Logger::ok("VNC Xsession 配置完成");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // VNC 命令构建
    // ═══════════════════════════════════════════════════════════════

    std::string GUIManager::build_vnc_start_command(int display, int width, int height) const {
        if (display <= 0) display = vnc_config_.display;
        if (width <= 0) width = vnc_config_.resolution_w;
        if (height <= 0) height = vnc_config_.resolution_h;

        std::ostringstream cmd;

        if (vnc_config_.server == "tight") {
            // TightVNC: tightvncserver :display -geometry WxH -depth 24
            cmd << "tightvncserver :" << display
                    << " -geometry " << width << "x" << height
                    << " -depth " << vnc_config_.pixel_depth;
            if (!vnc_config_.password.empty()) {
                cmd << " -passwd " << vnc_config_.passwd_file.string();
            }
            if (vnc_config_.always_shared) {
                cmd << " -alwaysshared";
            }
        } else {
            // TigerVNC: vncserver :display -geometry WxH -depth 24 -localhost no
            cmd << "vncserver :" << display
                    << " -geometry " << width << "x" << height
                    << " -depth " << vnc_config_.pixel_depth
                    << " -localhost no"
                    << " -SecurityTypes VncAuth";
            if (!vnc_config_.password.empty()) {
                cmd << " -rfbauth " << vnc_config_.passwd_file.string();
            }
            if (vnc_config_.zlib_level >= 0) {
                cmd << " -ZlibLevel " << vnc_config_.zlib_level;
            }
            if (vnc_config_.always_shared) {
                cmd << " -AlwaysShared";
            }
        }

        return cmd.str();
    }

    std::string GUIManager::build_xvfb_command(int display, int width, int height) const {
        if (display <= 0) display = 233; // x11vnc 默认使用 233
        if (width <= 0) width = vnc_config_.resolution_w;
        if (height <= 0) height = vnc_config_.resolution_h;

        std::ostringstream cmd;
        cmd << "Xvfb :" << display
                << " -screen 0 " << width << "x" << height << "x24"
                << " -ac +extension RANDR &";
        return cmd.str();
    }

    std::string GUIManager::build_x11vnc_command(int display) const {
        if (display <= 0) display = 233;

        std::ostringstream cmd;
        cmd << "x11vnc -display :" << display
                << " -ncache_cr -xkb -noxrecord -noxdamage"
                << " -forever -bg -noshm -cursor arrow"
                << " -rfbport " << vnc_config_.rfb_port;

        if (!vnc_config_.password.empty()) {
            cmd << " -rfbauth " << vnc_config_.passwd_file.string();
        }

        return cmd.str();
    }

    // ═══════════════════════════════════════════════════════════════
    // VNC 启动/停止
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::start_vnc(int display, int width, int height) {
        Logger::step(_("gui.vnc.start"));

        if (display > 0) {
            vnc_config_.display = display;
            vnc_config_.update_port();
        }
        if (width > 0) vnc_config_.resolution_w = width;
        if (height > 0) vnc_config_.resolution_h = height;

        // 检查 Xvnc
        if (!check_xvnc_command()) {
            Logger::error("VNC 服务端不可用");
            return false;
        }

        // 确保 xstartup 存在
        if (!fs::exists(vnc_config_.xstartup_file)) {
            Logger::warn("xstartup 不存在，创建默认配置...");
            configure_xstartup("xfce");
        }

        // 如果已经运行则提示
        if (is_vnc_running()) {
            Logger::warn("VNC display :" + std::to_string(vnc_config_.display) + " 已在运行中");
            return true;
        }

        // 清理残留锁文件
        std::string lock_path = "/tmp/.X" + std::to_string(vnc_config_.display) + "-lock";
        std::string socket_path = "/tmp/.X11-unix/X" + std::to_string(vnc_config_.display);
        Executor::shell("rm -f " + lock_path + " " + socket_path + " 2>/dev/null");

        // 启动 D-Bus
        launch_dbus_daemon();

        // 设置环境变量并启动 VNC
        std::string cmd = build_vnc_start_command(display, width, height);
        ExecResult result = Executor::shell(cmd + " > /tmp/tmoe_vnc_startup.log 2>&1");

        if (result.ok()) {
            Logger::ok(_f("gui.vnc.started",
                          std::to_string(vnc_config_.rfb_port) + " (display :" +
                          std::to_string(vnc_config_.display) + ")"));
            Logger::info("分辨率: " + std::to_string(vnc_config_.resolution_w) + "x" +
                         std::to_string(vnc_config_.resolution_h));
            Logger::info("连接地址: " + get_vnc_connection_uri());
            return true;
        }

        Logger::error("VNC 启动失败，查看日志: /tmp/tmoe_vnc_startup.log");
        return false;
    }

    bool GUIManager::stop_vnc(int display) {
        Logger::step(_("gui.vnc.stop"));

        if (display <= 0) display = vnc_config_.display;

        // 使用 vncserver -kill 清理 (TigerVNC)
        std::string cmd = "vncserver -kill :" + std::to_string(display) + " 2>/dev/null";
        Executor::shell(cmd);

        // 备用: pkill
        Executor::shell("pkill -f 'Xvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        Executor::shell("pkill -f 'Xtigervnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        Executor::shell("pkill -f 'Xtightvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");

        // 清理锁文件
        Executor::shell("rm -f /tmp/.X" + std::to_string(display) + "-lock 2>/dev/null");
        Executor::shell("rm -f /tmp/.X11-unix/X" + std::to_string(display) + " 2>/dev/null");

        Logger::ok("VNC display :" + std::to_string(display) + " 已停止");
        return true;
    }

    bool GUIManager::start_x11vnc(int display) {
        Logger::step("启动 x11vnc 服务 (Xvfb)");

        int x11_display = (display > 0) ? display : 233;

        // 1. 启动 Xvfb
        std::string xvfb_cmd = build_xvfb_command(x11_display, 0, 0);
        Logger::debug("执行: " + xvfb_cmd);
        Executor::shell(xvfb_cmd);

        // 等待 Xvfb 就绪
        Executor::shell("sleep 2");

        // 2. 启动 x11vnc
        std::string x11vnc_cmd = build_x11vnc_command(x11_display);
        Logger::debug("执行: " + x11vnc_cmd);
        ExecResult result = Executor::shell(x11vnc_cmd);

        if (result.ok()) {
            Logger::ok("x11vnc 已启动 — 端口 " + std::to_string(vnc_config_.rfb_port));
            return true;
        }

        Logger::error("x11vnc 启动失败");
        return false;
    }

    bool GUIManager::stop_x11vnc() {
        Logger::step("停止 x11vnc");
        Executor::shell("pkill -f x11vnc 2>/dev/null || true");
        Executor::shell("pkill Xvfb 2>/dev/null || true");
        Executor::shell("rm -f /tmp/.X233-lock /tmp/.X11-unix/X233 2>/dev/null || true");
        Logger::ok("x11vnc / Xvfb 已停止");
        return true;
    }

    bool GUIManager::kill_all_vnc() {
        Logger::step("清理所有 VNC 进程...");
        Executor::shell("pkill Xtightvnc 2>/dev/null || true");
        Executor::shell("pkill Xtigervnc 2>/dev/null || true");
        Executor::shell("pkill Xvnc 2>/dev/null || true");
        Executor::shell("pkill x11vnc 2>/dev/null || true");
        Executor::shell("pkill Xvfb 2>/dev/null || true");

        // 清理所有锁文件
        Executor::shell("rm -f /tmp/.X*-lock /tmp/.X11-unix/X* 2>/dev/null || true");
        Logger::ok("所有 VNC 进程已清理");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面环境管理
    // ═══════════════════════════════════════════════════════════════

    DesktopInfo GUIManager::get_desktop_info(std::string_view desktop) const {
        for (const auto &info: desktop_registry()) {
            if (info.id == desktop) return info;
        }
        // 默认回退到 xfce
        return desktop_registry().front();
    }

    std::vector<DesktopInfo> GUIManager::list_desktops() const {
        return desktop_registry();
    }

    bool GUIManager::install_desktop(std::string_view desktop) {
        DesktopInfo info = get_desktop_info(desktop);
        Logger::step(_f("gui.install.desktop", info.name));

        // 检查是否需要 root 权限
        if (info.requires_root && !cfg_.is_root) {
            Logger::warn(info.name + " 建议在 root 环境下安装 (需要 systemd 支持)");
            if (!Logger::confirm("是否继续尝试安装？(可能部分功能不可用)")) {
                return false;
            }
        }

        // 安装桌面环境包
        std::vector<std::string> pkgs;
        std::istringstream iss(info.pkg_group);
        std::string pkg;
        while (iss >> pkg) pkgs.emplace_back(pkg);

        // 附加通用包
        pkgs.emplace_back("dbus-x11");

        if (!install_packages(pkgs)) {
            Logger::error(_f("gui.install.fail", info.name));
            return false;
        }

        // 配置 VNC xstartup
        if (!configure_xstartup(desktop)) {
            Logger::warn("xstartup 配置有部分问题，但桌面环境已安装");
        }

        Logger::ok(_f("gui.install.success", info.name));

        // 显示兼容性说明
        if (!info.compat_notes.empty()) {
            Logger::info("兼容性说明: " + info.compat_notes);
        }

        // 根据桌面类型推荐 VNC 服务端
        std::string desktop_lower(desktop);
        std::transform(desktop_lower.begin(), desktop_lower.end(), desktop_lower.begin(), ::tolower);

        if (desktop_lower == "kde" || desktop_lower == "gnome" || desktop_lower == "cinnamon" ||
            desktop_lower == "dde" || desktop_lower == "ukui" || desktop_lower == "budgie") {
            Logger::info("💡 强烈推荐使用 TigerVNC 以获得最佳兼容性");
            vnc_config_.server = "tiger";
            vnc_config_.server_bin = "tigervnc";
        }

        return true;
    }

    bool GUIManager::install_window_manager(std::string_view wm) {
        const auto &registry = window_manager_registry();
        auto it = registry.find(std::string(wm));
        if (it == registry.end()) {
            Logger::error("不支持的窗口管理器: " + std::string(wm));
            return false;
        }

        Logger::step("安装窗口管理器: " + std::string(wm));

        std::vector<std::string> pkgs;
        std::istringstream iss(it->second);
        std::string pkg;
        while (iss >> pkg) pkgs.emplace_back(pkg);

        return install_packages(pkgs);
    }

    std::vector<std::string> GUIManager::list_window_managers() const {
        std::vector<std::string> names;
        for (const auto &[name, _]: window_manager_registry()) {
            names.emplace_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    // ═══════════════════════════════════════════════════════════════
    // noVNC (HTML5 VNC 客户端)
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_novnc() {
        Logger::step("安装 noVNC...");

        // 检查是否已安装
        if (fs::exists("/usr/share/novnc") || Executor::has("websockify")) {
            Logger::ok("noVNC 已安装");
            return true;
        }

        // 安装依赖
        std::vector<std::string> pkgs = {"novnc", "websockify", "python3-numpy", "python3-websockify"};
        for (const auto &pkg: pkgs) {
            Executor::shell(cfg_.install_command + " " + pkg + " 2>/dev/null || true");
        }

        // 如果 apt 源没有 novnc，从 git 安装
        if (!fs::exists("/usr/share/novnc")) {
            Logger::info("从 GitHub 克隆 noVNC...");
            Executor::shell("git clone --depth=1 https://github.com/novnc/noVNC.git "
                "/opt/novnc 2>/dev/null || true");
            if (fs::exists("/opt/novnc")) {
                // 创建符号链接
                Executor::shell("ln -sf /opt/novnc /usr/share/novnc 2>/dev/null || true");
            }
        }

        if (fs::exists("/usr/share/novnc") || fs::exists("/opt/novnc")) {
            Logger::ok("noVNC 安装完成");
            return true;
        }

        Logger::error("noVNC 安装失败");
        return false;
    }

    bool GUIManager::configure_novnc() {
        Logger::step(_("gui.novnc.config"));

        // 选择端口
        std::string port_cmd = cfg_.tui_bin +
                               " --title \"noVNC 端口\" --menu \"请选择 noVNC 端口:\" 0 0 0 "
                               "\"36080\" \"默认端口 36080\" "
                               "\"36081\" \"备用端口 36081\" "
                               "\"6080\" \"标准端口 6080\" "
                               "\"custom\" \"自定义端口\"";

        std::string choice = Executor::tui_select(port_cmd);
        if (choice == "custom") {
            std::string input_cmd = cfg_.tui_bin +
                                    " --title \"自定义端口\" --inputbox \"请输入 noVNC HTTP 端口:\" 10 40 \"36080\"";
            auto result = Executor::shell(input_cmd + " 2>&1");
            try { novnc_port_ = std::stoi(result.stdout_data); } catch (...) { novnc_port_ = 36080; }
        } else if (!choice.empty()) {
            try { novnc_port_ = std::stoi(choice); } catch (...) { novnc_port_ = 36080; }
        }

        Logger::info("noVNC 端口设置为: " + std::to_string(novnc_port_));
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
        if (!is_vnc_running()) {
            Logger::info("VNC 未运行，正在启动...");
            if (!start_vnc()) return false;
        }

        // 启动 websockify 代理
        std::ostringstream cmd;
        cmd << "websockify --web=" << novnc_dir << " "
                << novnc_port_ << " localhost:" << vnc_config_.rfb_port
                << " > /tmp/tmoe_novnc.log 2>&1 &";

        Executor::shell(cmd.str());
        Executor::shell("sleep 2");

        Logger::ok(_f("gui.novnc.url", get_novnc_url()));
        Logger::info("VNC 后端端口: " + std::to_string(vnc_config_.rfb_port));
        return true;
    }

    std::string GUIManager::get_novnc_url() const {
        return "http://localhost:" + std::to_string(novnc_port_) + "/vnc.html";
    }

    // ═══════════════════════════════════════════════════════════════
    // XRDP
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_xrdp() {
        Logger::step("安装 XRDP...");

        std::vector<std::string> pkgs = {"xrdp", "xorgxrdp"};
        if (!install_packages(pkgs)) {
            Logger::error("XRDP 安装失败");
            return false;
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
        write_file("/usr/share/polkit-1/actions/xrdp-sesman.policy", polkit_rule);

        // 配置 startwm.sh
        configure_xrdp_session("xfce");

        // 启动
        Executor::shell("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null || true");

        Logger::ok("XRDP 安装完成，默认端口: 3389");
        return true;
    }

    bool GUIManager::configure_xrdp_session(std::string_view desktop) {
        DesktopInfo info = get_desktop_info(desktop);

        std::string wm_content = "#!/bin/sh\n"
                                 "# tmoe-linux XRDP startwm — 自动生成\n\n"
                                 "if command -v " + info.session_cmd1 + " >/dev/null 2>&1; then\n"
                                 "    exec " + info.session_cmd1 + "\n"
                                 "elif command -v " + info.session_cmd2 + " >/dev/null 2>&1; then\n"
                                 "    exec " + info.session_cmd2 + "\n"
                                 "else\n"
                                 "    exec xterm\n"
                                 "fi\n";

        return write_file("/etc/xrdp/startwm.sh", wm_content) &&
               Executor::shell("chmod +x /etc/xrdp/startwm.sh").ok();
    }

    bool GUIManager::start_xrdp() {
        Logger::step("启动 XRDP 服务...");
        if (Executor::shell("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null").ok()) {
            Logger::ok("XRDP 已启动 — 端口 3389");
            return true;
        }
        // 备用: 直接启动 xrdp 守护进程
        if (Executor::shell("xrdp 2>/dev/null &").ok()) {
            Logger::ok("XRDP 已启动 (直接模式)");
            return true;
        }
        Logger::error("XRDP 启动失败");
        return false;
    }

    bool GUIManager::stop_xrdp() {
        Logger::step("停止 XRDP...");
        Executor::shell("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || "
            "pkill xrdp 2>/dev/null || true");
        Logger::ok("XRDP 已停止");
        return true;
    }

    bool GUIManager::restart_xrdp() {
        stop_xrdp();
        Executor::shell("sleep 1");
        bool ok = start_xrdp();
        if (ok) {
            Logger::info("连接地址: rdp://127.0.0.1:3389");
            Logger::info("WSL 用户请使用 Windows 远程桌面连接 localhost:3389");
        }
        return ok;
    }

    bool GUIManager::set_xrdp_port(int port) {
        // 修改 /etc/xrdp/xrdp.ini 中的 port 行
        std::string cmd = "sed -i 's/^port=.*/port=" + std::to_string(port) +
                          "/' /etc/xrdp/xrdp.ini 2>/dev/null";
        if (Executor::shell(cmd).ok()) {
            Logger::ok("XRDP 端口已修改为: " + std::to_string(port));
            Logger::info("需要重启 XRDP 使配置生效");
            return true;
        }
        Logger::error("修改 XRDP 端口失败");
        return false;
    }

    bool GUIManager::remove_xrdp() {
        Logger::step("卸载 XRDP...");
        Executor::shell(cfg_.remove_command + " xrdp xorgxrdp 2>/dev/null || true");
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
                                  " --title \"XSDL 配置\" --menu \"请配置 DISPLAY 地址:\" 0 0 0 "
                                  "\"1\" \"127.0.0.1:0 (本地 X Server)\" "
                                  "\"2\" \"自定义 IP:显示编号\"";

        std::string choice = Executor::tui_select(display_cmd);
        std::string display = "127.0.0.1:0";

        if (choice == "2") {
            std::string input_cmd = cfg_.tui_bin +
                                    " --title \"自定义 DISPLAY\" --inputbox \"请输入 DISPLAY 地址:\" 10 40 \"127.0.0.1:0\"";
            auto result = Executor::shell(input_cmd + " 2>&1");
            if (!result.stdout_data.empty()) {
                display = result.stdout_data;
                while (!display.empty() && (display.back() == '\n' || display.back() == '\r'))
                    display.pop_back();
            }
        }

        Executor::shell("export DISPLAY=" + display);
        Logger::ok("XSDL DISPLAY 设置为: " + display);
        return true;
    }

    bool GUIManager::start_xsdl() {
        Logger::step("启动 XSDL/VcXsrv 模式...");

        if (cfg_.is_wsl) {
            detect_wsl_environment();
        }

        // 设置 DISPLAY
        std::string display = cfg_.is_wsl ? (vnc_config_.windows_ip + ":0") : "127.0.0.1:0";
        std::string env = "export DISPLAY=" + display + " "
                          "export PULSE_SERVER=tcp:" + display.substr(0, display.find(':')) + ":" +
                          std::to_string(vnc_config_.pulse_port);

        // WSL 下启动 VcXsrv
        if (cfg_.is_wsl) {
            Logger::info("检测到 WSL 环境，请在 Windows 上启动 VcXsrv");
            Logger::info("DISPLAY=" + display);
        }

        // 尝试启动桌面会话
        std::string cmd = env + " xfce4-session 2>/dev/null || " + env + " startxfce4 2>/dev/null || "
                          + env + " openbox 2>/dev/null &";
        Executor::shell(cmd);

        Logger::ok("XSDL 模式已启动 — DISPLAY=" + display);
        return true;
    }

    void GUIManager::detect_wsl_environment() {
        // WSL2: 从路由表获取网关 IP
        auto route = Executor::shell("ip route list table 0 | head -n 1 | awk '{print $3}' 2>/dev/null");
        if (route.ok() && !route.stdout_data.empty()) {
            std::string ip = route.stdout_data;
            while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r')) ip.pop_back();
            vnc_config_.windows_ip = ip;
        } else {
            // WSL1: localhost
            vnc_config_.windows_ip = "127.0.0.1";
        }

        // 检查是 WSL1 还是 WSL2
        auto wsl_ver = Executor::shell("uname -r | grep -qi microsoft && echo 'wsl1' || echo 'wsl2'");
        if (wsl_ver.stdout_data.find("wsl1") != std::string::npos) {
            Logger::info("检测到 WSL1: DISPLAY=localhost:0");
            vnc_config_.windows_ip = "127.0.0.1";
        } else {
            Logger::info("检测到 WSL2, 网关 IP: " + vnc_config_.windows_ip);
        }
    }

    bool GUIManager::configure_wsl_pulseaudio() {
        Logger::step("配置 WSL PulseAudio...");

        if (!cfg_.is_wsl) {
            Logger::warn("当前不在 WSL 环境中");
            return false;
        }

        detect_wsl_environment();

        std::string pulse_server = "tcp:" + vnc_config_.windows_ip + ":" +
                                   std::to_string(vnc_config_.pulse_port);

        // 写入 ~/.bashrc 或配置文件
        std::string bashrc = std::getenv("HOME")
                                 ? std::string(std::getenv("HOME")) + "/.bashrc"
                                 : "/root/.bashrc";
        append_file(fs::path(bashrc),
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

        std::string cmd = "Xwayland :" + std::to_string(display_port) + " -noreset &";
        Executor::shell(cmd);
        Executor::shell("sleep 1");
        Executor::shell("export DISPLAY=:" + std::to_string(display_port));

        Logger::ok("WSLg Xwayland 已启动 — DISPLAY=:" + std::to_string(display_port));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // D-Bus 守护进程
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::launch_dbus_daemon() {
        // 检查 dbus 是否已在运行
        if (fs::exists("/var/run/dbus/pid") || fs::exists("/run/dbus/pid")) {
            return true;
        }

        Logger::debug("启动 D-Bus 守护进程...");

        // 尝试 service/systemctl
        if (Executor::shell("service dbus start 2>/dev/null").ok()) return true;
        if (Executor::shell("systemctl start dbus 2>/dev/null").ok()) return true;

        // 直接启动 dbus-daemon
        Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
        if (Executor::shell("dbus-daemon --system --fork 2>/dev/null").ok()) return true;

        Logger::debug("使用 dbus-launch 启动会话 dbus");
        return Executor::shell("dbus-launch --exit-with-session 2>/dev/null &").ok();
    }

    bool GUIManager::fix_vnc_dbus() {
        Logger::step("修复 VNC dbus-launch...");

        // 确保 dbus 目录和 machine-id 正确
        Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");

        if (!fs::exists("/etc/machine-id")) {
            Executor::shell("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
                "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
        }
        if (!fs::exists("/var/lib/dbus/machine-id")) {
            Executor::shell("cp /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
        }

        Logger::ok("dbus 修复完成");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 环境检测
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::detect_android_resolution(int &width, int &height) const {
        if (!cfg_.is_termux) return false;

        auto result = Executor::shell("wm size 2>/dev/null");
        if (!result.ok() || result.stdout_data.empty()) return false;

        std::string output = result.stdout_data;
        // 解析 "Physical size: 1080x2340" 或 "Override size: 1080x2340"
        auto pos = output.find(':');
        if (pos == std::string::npos) return false;

        std::string size_str = output.substr(pos + 1);
        // 去除空格
        size_str.erase(0, size_str.find_first_not_of(" \t\n\r"));
        auto x_pos = size_str.find('x');
        if (x_pos == std::string::npos) return false;

        try {
            width = std::stoi(size_str.substr(0, x_pos));
            height = std::stoi(size_str.substr(x_pos + 1));

            // Android 手机通常是竖屏 (w < h)，横过来用
            if (width < height) {
                // VNC 使用横屏分辨率 (宽的作为高度，高的作为宽度)
                int tmp = width;
                width = height / 2;
                height = tmp / 2;
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面美化
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::beautify_desktop() {
        Logger::step("桌面美化...");
        return true; // 由 TUI 菜单驱动
    }

    bool GUIManager::install_theme(std::string_view theme) {
        Logger::step("安装桌面主题: " + std::string(theme));

        // 通用 GTK/XFCE 主题包
        std::string pkg_name;
        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        if (name_lower.find("arc") != std::string::npos) pkg_name = "arc-theme";
        else if (name_lower.find("adapta") != std::string::npos) pkg_name = "adapta-gtk-theme";
        else if (name_lower.find("numix") != std::string::npos) pkg_name = "numix-gtk-theme";
        else if (name_lower.find("papirus") != std::string::npos) pkg_name = "papirus-icon-theme";
        else if (name_lower.find("breeze") != std::string::npos) pkg_name = "breeze-gtk-theme";
        else if (name_lower.find("materia") != std::string::npos) pkg_name = "materia-gtk-theme";
        else pkg_name = name_lower + "-theme";

        return Executor::shell(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok();
    }

    bool GUIManager::install_icon_theme(std::string_view theme) {
        Logger::step("安装图标主题: " + std::string(theme));

        std::string name_lower(theme);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        std::string pkg_name;
        if (name_lower.find("papirus") != std::string::npos) pkg_name = "papirus-icon-theme";
        else if (name_lower.find("numix") != std::string::npos) pkg_name = "numix-icon-theme";
        else if (name_lower.find("breeze") != std::string::npos) pkg_name = "breeze-icon-theme";
        else if (name_lower.find("tango") != std::string::npos) pkg_name = "tango-icon-theme";
        else if (name_lower.find("adwaita") != std::string::npos) pkg_name = "adwaita-icon-theme";
        else if (name_lower.find("elementary") != std::string::npos) pkg_name = "elementary-xfce-icon-theme";
        else if (name_lower.find("moka") != std::string::npos) pkg_name = "moka-icon-theme";
        else if (name_lower.find("faenza") != std::string::npos) pkg_name = "faenza-icon-theme";
        else pkg_name = name_lower + "-icon-theme";

        return Executor::shell(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok();
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
        return install_packages({"plank"});
    }

    // ═══════════════════════════════════════════════════════════════
    // PulseAudio (保留现有实现，略作优化)
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::start_pulseaudio_bridge() const {
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

        return Executor::shell(cmd + " >/dev/null 2>&1").ok();
    }

    // ═══════════════════════════════════════════════════════════════
    // TUI 交互式菜单
    // ═══════════════════════════════════════════════════════════════

    void GUIManager::run_gui_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"🖥️ GUI/VNC 远程桌面管理\""
                                   " --menu \"请选择操作:\\n\\n当前配置: " + vnc_config_.server +
                                   " — 端口 " + std::to_string(vnc_config_.rfb_port) +
                                   " — 分辨率 " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + "\" 0 0 0 "
                                   "\"1\" \"安装桌面环境 (XFCE/LXDE/MATE/KDE/GNOME...)\" "
                                   "\"2\" \"安装/配置 VNC 服务端\" "
                                   "\"3\" \"启动 VNC 服务\" "
                                   "\"4\" \"停止 VNC 服务\" "
                                   "\"5\" \"noVNC (HTML5 浏览器访问)\" "
                                   "\"6\" \"XRDP (RDP 协议远程桌面)\" "
                                   "\"7\" \"XSDL/VcXsrv (WSL X Server)\" "
                                   "\"8\" \"修改 VNC 配置 (密码/分辨率/端口...)\" "
                                   "\"9\" \"桌面美化 (主题/图标/壁纸)\" "
                                   "\"0\" \"返回上级菜单\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_desktop_install_menu();
            } else if (choice == "2") {
                install_vnc_server();
                choose_vnc_server();
                configure_vnc_password();
                Logger::press_enter();
            } else if (choice == "3") {
                start_vnc();
                Logger::press_enter();
            } else if (choice == "4") {
                if (Logger::confirm("确认停止 VNC 服务？")) {
                    stop_vnc();
                }
                Logger::press_enter();
            } else if (choice == "5") {
                install_novnc();
                configure_novnc();
                start_novnc();
                Logger::press_enter();
            } else if (choice == "6") {
                run_remote_desktop_menu();
            } else if (choice == "7") {
                if (cfg_.is_wsl) {
                    configure_xsdl();
                    start_xsdl();
                } else {
                    Logger::info("XSDL/VcXsrv 主要用于 WSL 环境");
                    Logger::info("在 Linux 桌面直接运行 X 应用无需此配置");
                }
                Logger::press_enter();
            } else if (choice == "8") {
                run_vnc_config_menu();
            } else if (choice == "9") {
                run_beautification_menu();
            }
        }
    }

    void GUIManager::run_vnc_config_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"⚙️ VNC 配置修改\" --menu \"当前: " +
                                   vnc_config_.server + " — 端口 " + std::to_string(vnc_config_.rfb_port) +
                                   " — " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + "\" 0 0 0 "
                                   "\"1\" \"VNC 密码\" "
                                   "\"2\" \"切换 Tiger/Tight VNC 服务端\" "
                                   "\"3\" \"修改分辨率 (当前: " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + ")\" "
                                   "\"4\" \"修改 VNC 端口 (当前: " + std::to_string(vnc_config_.rfb_port) + ")\" "
                                   "\"5\" \"像素深度 (当前: " + std::to_string(vnc_config_.pixel_depth) + ")\" "
                                   "\"6\" \"Zlib 压缩级别 (当前: " + std::to_string(vnc_config_.zlib_level) + ")\" "
                                   "\"7\" \"PulseAudio 音频配置\" "
                                   "\"8\" \"修复 dbus-launch\" "
                                   "\"9\" \"清理所有 VNC 进程\" "
                                   "\"0\" \"返回\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                configure_vnc_password();
            } else if (choice == "2") {
                choose_vnc_server();
                configure_vnc_defaults();
            } else if (choice == "3") {
                // 分辨率选择
                std::string res_cmd = cfg_.tui_bin +
                                      " --title \"分辨率\" --menu \"选择分辨率:\" 0 0 0 "
                                      "\"1440x720\"  \"默认 18:9\" "
                                      "\"1280x720\"  \"HD 16:9\" "
                                      "\"1920x1080\" \"FHD 16:9\" "
                                      "\"2560x1440\" \"2K 16:9\" "
                                      "\"1024x768\"  \"4:3 传统\" "
                                      "\"custom\"    \"自定义\"";
                std::string res = Executor::tui_select(res_cmd);
                if (!res.empty() && res != "custom") {
                    auto xpos = res.find('x');
                    if (xpos != std::string::npos) {
                        vnc_config_.resolution_w = std::stoi(res.substr(0, xpos));
                        vnc_config_.resolution_h = std::stoi(res.substr(xpos + 1));
                        configure_vnc_defaults();
                    }
                }
            } else if (choice == "4") {
                std::string port_cmd = cfg_.tui_bin +
                                       " --title \"VNC 端口\" --menu \"选择端口:\" 0 0 0 "
                                       "\"5902\" \"默认 (display :2)\" "
                                       "\"5903\" \"备用 (display :3)\" "
                                       "\"5901\" \"display :1\"";
                std::string port = Executor::tui_select(port_cmd);
                if (!port.empty()) {
                    int p = std::stoi(port);
                    vnc_config_.display = p - 5900;
                    vnc_config_.rfb_port = p;
                }
            } else if (choice == "5") {
                std::string depth_cmd = cfg_.tui_bin +
                                        " --title \"像素深度\" --menu \"选择:\" 0 0 0 "
                                        "\"24\" \"24位真彩色 (推荐)\" "
                                        "\"16\" \"16位 (省带宽)\"";
                std::string depth = Executor::tui_select(depth_cmd);
                if (!depth.empty()) vnc_config_.pixel_depth = std::stoi(depth);
            } else if (choice == "6") {
                std::string zlib_cmd = cfg_.tui_bin +
                                       " --title \"Zlib 压缩\" --menu \"0=最快 9=最优压缩:\" 0 0 0 "
                                       "\"0\" \"级别 0 — 不压缩 (最快)\" "
                                       "\"3\" \"级别 3 — 平衡\" "
                                       "\"6\" \"级别 6 — 较优\" "
                                       "\"9\" \"级别 9 — 最优压缩 (慢速连接推荐)\"";
                std::string zlib = Executor::tui_select(zlib_cmd);
                if (!zlib.empty()) vnc_config_.zlib_level = std::stoi(zlib);
            } else if (choice == "7") {
                start_pulseaudio_bridge();
            } else if (choice == "8") {
                fix_vnc_dbus();
            } else if (choice == "9") {
                if (Logger::confirm("确认终止所有 VNC 进程？")) {
                    kill_all_vnc();
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_desktop_install_menu() {
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"📦 安装桌面环境\""
                               " --menu \"请选择要安装的桌面环境:\\n\\n"
                               "🟢 = 支持 proot 无 root 运行  🔴 = 需要 root/systemd\\n\" 0 0 0 ";

        auto desktops = list_desktops();
        for (size_t i = 0; i < desktops.size(); ++i) {
            std::string prefix = desktops[i].requires_root ? "🔴 " : "🟢 ";
            std::string label = std::to_string(i + 1);
            std::string desc = prefix + " " + desktops[i].name;
            if (!desktops[i].compat_notes.empty())
                desc += " — " + desktops[i].compat_notes;
            menu_cmd += "\"" + label + "\" \"" + desc + "\" ";
        }

        // 窗口管理器选项
        menu_cmd += "\"wm\" \"📦 安装窗口管理器 (openbox/i3/awesome/icewm...)\" ";
        menu_cmd += "\"0\" \"返回\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice == "0" || choice.empty()) return;

        if (choice == "wm") {
            // 窗口管理器子菜单
            std::string wm_menu = cfg_.tui_bin +
                                  " --title \"窗口管理器\" --menu \"选择窗口管理器:\" 0 0 0 ";
            int idx = 1;
            for (const auto &name: list_window_managers()) {
                wm_menu += "\"" + std::to_string(idx++) + "\" \"" + name + "\" ";
            }
            wm_menu += "\"0\" \"返回\"";
            std::string wm_choice = Executor::tui_select(wm_menu);
            if (wm_choice != "0" && !wm_choice.empty()) {
                auto wms = list_window_managers();
                int wm_idx = std::stoi(wm_choice) - 1;
                if (wm_idx >= 0 && wm_idx < static_cast<int>(wms.size())) {
                    install_window_manager(wms[wm_idx]);
                }
            }
        } else {
            int idx = std::stoi(choice) - 1;
            if (idx >= 0 && idx < static_cast<int>(desktops.size())) {
                install_desktop(desktops[idx].id);
            }
        }
        Logger::press_enter();
    }

    void GUIManager::run_remote_desktop_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"🌐 远程桌面协议\" --menu \"请选择协议:\" 0 0 0 "
                                   "\"1\" \"noVNC (HTML5 Web 客户端)\" "
                                   "\"2\" \"x11vnc (基于 Xvfb 的 VNC)\" "
                                   "\"3\" \"XRDP (RDP 协议)\" "
                                   "\"4\" \"XSDL / VcXsrv (WSL)\" "
                                   "\"0\" \"返回\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                install_novnc();
                configure_novnc();
                start_novnc();
            } else if (choice == "2") {
                if (Logger::confirm("启动 x11vnc？将先停止可能存在的 VNC 服务。")) {
                    stop_vnc();
                    start_x11vnc();
                }
            } else if (choice == "3") {
                // XRDP 子菜单
                while (true) {
                    std::string xrdp_menu = cfg_.tui_bin +
                                            " --title \"XRDP 管理\" --menu \"XRDP 操作:\" 0 0 0 "
                                            "\"1\" \"一键安装+配置 XRDP\" "
                                            "\"2\" \"启动 XRDP\" "
                                            "\"3\" \"停止 XRDP\" "
                                            "\"4\" \"重启 XRDP\" "
                                            "\"5\" \"修改端口\" "
                                            "\"6\" \"卸载 XRDP\" "
                                            "\"0\" \"返回\"";
                    std::string xchoice = Executor::tui_select(xrdp_menu);
                    if (xchoice == "0" || xchoice.empty()) break;
                    if (xchoice == "1") install_xrdp();
                    else if (xchoice == "2") start_xrdp();
                    else if (xchoice == "3") stop_xrdp();
                    else if (xchoice == "4") restart_xrdp();
                    else if (xchoice == "5") {
                        std::string port_cmd = cfg_.tui_bin +
                                               " --title \"XRDP 端口\" --inputbox \"输入端口号:\" 10 40 \"3389\"";
                        auto result = Executor::shell(port_cmd + " 2>&1");
                        if (!result.stdout_data.empty()) {
                            try {
                                set_xrdp_port(std::stoi(result.stdout_data));
                            } catch (...) {
                            }
                        }
                    } else if (xchoice == "6") remove_xrdp();
                    Logger::press_enter();
                }
            } else if (choice == "4") {
                if (cfg_.is_wsl) {
                    start_xsdl();
                } else {
                    Logger::info("XSDL/VcXsrv 主要用于 WSL 环境");
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_beautification_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"🎨 桌面美化\" --menu \"请选择:\" 0 0 0 "
                                   "\"1\" \"安装 GTK 主题\" "
                                   "\"2\" \"安装图标主题\" "
                                   "\"3\" \"设置壁纸\" "
                                   "\"4\" \"安装 Plank dock\" "
                                   "\"5\" \"安装 Compiz 特效\" "
                                   "\"0\" \"返回\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                std::string theme_menu = cfg_.tui_bin +
                                         " --title \"GTK 主题\" --menu \"选择主题:\" 0 0 0 "
                                         "\"arc\" \"Arc (流行简洁)\" "
                                         "\"adapta\" \"Adapta (Material Design)\" "
                                         "\"numix\" \"Numix (现代扁平)\" "
                                         "\"materia\" \"Materia (Material)\" "
                                         "\"breeze\" \"Breeze (KDE 风格)\"";
                std::string t = Executor::tui_select(theme_menu);
                if (!t.empty() && t != "0") install_theme(t);
            } else if (choice == "2") {
                std::string icon_menu = cfg_.tui_bin +
                                        " --title \"图标主题\" --menu \"选择:\" 0 0 0 "
                                        "\"papirus\" \"Papirus (推荐，图标最全)\" "
                                        "\"numix\" \"Numix Circle\" "
                                        "\"breeze\" \"Breeze\" "
                                        "\"elementary\" \"elementary Xfce\" "
                                        "\"tango\" \"Tango 经典\" "
                                        "\"moka\" \"Moka + Faba\"";
                std::string i = Executor::tui_select(icon_menu);
                if (!i.empty() && i != "0") install_icon_theme(i);
            } else if (choice == "3") {
                set_wallpaper("");
            } else if (choice == "4") {
                install_dock();
            } else if (choice == "5") {
                install_packages({"compiz", "compiz-core", "compiz-plugins"});
            }
            Logger::press_enter();
        }
    }

    bool GUIManager::auto_install_gui(std::string_view desktop) {
        Logger::info("自动安装 GUI 模式: " + std::string(desktop));

        if (!install_vnc_server()) return false;
        if (!install_desktop(desktop)) return false;
        if (!configure_vnc_password("tmoe123")) return false;

        // 根据桌面推荐服务端
        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (d == "kde" || d == "gnome" || d == "cinnamon" || d == "dde") {
            vnc_config_.server = "tiger";
        }

        return start_vnc();
    }

    // ═══════════════════════════════════════════════════════════════
    // 运行时状态查询
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::is_vnc_running(int display) const {
        if (display <= 0) display = vnc_config_.display;

        // 检查进程
        auto result = Executor::shell("pgrep -f 'X(vnc|tigervnc|tightvnc).*:" +
                                      std::to_string(display) + "' 2>/dev/null");
        if (result.ok() && !result.stdout_data.empty()) return true;

        // 检查锁文件
        if (fs::exists("/tmp/.X" + std::to_string(display) + "-lock")) return true;

        return false;
    }

    int GUIManager::detect_available_display() const {
        for (int d = 1; d <= 10; ++d) {
            if (!fs::exists("/tmp/.X" + std::to_string(d) + "-lock")) {
                return d;
            }
        }
        return 1;
    }

    std::string GUIManager::get_vnc_connection_uri() const {
        return "vnc://127.0.0.1:" + std::to_string(vnc_config_.rfb_port);
    }
} // namespace tmoe::domain
