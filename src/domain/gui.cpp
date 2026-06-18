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
        compare_fb = true;
        localhost_only = false;
        pulse_port = 4713;

        update_port();

        const char *home = std::getenv("HOME");
        fs::path home_dir = home ? fs::path(home) : fs::path("/root");
        vnc_home_dir = home_dir / ".vnc";
        xstartup_file = vnc_home_dir / "xstartup";
        passwd_file = vnc_home_dir / "passwd";
        xsession_file = "/etc/X11/xinit/Xsession";
        tigervnc_config = "/etc/tigervnc/vncserver-config-tmoe";
        vnc_pid_file = vnc_home_dir / "vnc.pid";
        x_pid_file = vnc_home_dir / "x.pid";

        // 从 os-release 读取桌面名称
        std::ifstream osr("/etc/os-release");
        if (osr.is_open()) {
            std::string line;
            while (std::getline(osr, line)) {
                if (line.rfind("PRETTY_NAME=", 0) == 0) {
                    auto s = line.find('"', 13);
                    auto e = line.rfind('"');
                    if (s != std::string::npos && e != std::string::npos && e > s) {
                        desktop_name = "tmoe-linux (" + line.substr(s + 1, e - s - 1) + ")";
                    }
                    break;
                }
            }
        }
        if (desktop_name.empty()) desktop_name = "tmoe-linux";

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

    /** 70+ 常用窗口管理器注册表。 */
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
            {"9wm", "9wm"}, {"aewm", "aewm"}, {"aewm++", "aewm++"},
            {"clfswm", "clfswm"}, {"ctwm", "ctwm"}, {"evilwm", "evilwm"},
            {"flwm", "flwm"}, {"lwm", "lwm"}, {"marco", "marco"},
            {"metacity", "metacity"}, {"miwm", "miwm"}, {"muffin", "muffin"},
            {"mwm", "mwm"}, {"oroborus", "oroborus"}, {"sapphire", "sapphire"},
            {"sawfish", "sawfish"}, {"subtl", "subtle"}, {"sugar", "sucrose"},
            {"tinywm", "tinywm"}, {"ukwm", "ukwm"}, {"vdesk", "vdesk"},
            {"vtwm", "vtwm"}, {"w9wm", "w9wm"}, {"wm2", "wm2"},
            {"xfwm4", "xfwm4"}, {"exwm", "exwm"},
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
        return Executor::passthrough(cmd).ok();
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
                               " --title \"" + std::string(_("gui.vnc_server_title")) + "\""
                               " --menu \"" + std::string(_("gui.vnc_server_prompt")) + "\" 0 0 0 "
                               "\"tiger\" \"" + std::string(_("gui.vnc_server_tiger")) + "\" "
                               "\"tight\" \"" + std::string(_("gui.vnc_server_tight")) + "\" ";

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
                                   " --title \"" + std::string(_("gui.vnc_passwd_title")) +
                                   "\" --passwordbox \"" + std::string(_("gui.vnc_passwd_prompt")) + "\" 10 40";
            auto result = Executor::passthrough(pass_cmd + " 2>&1");
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

        if (Executor::passthrough(cmd).ok()) {
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
        Logger::step("配置 TigerVNC 默认参数...");

        fs::create_directories(vnc_config_.tigervnc_config.parent_path());

        std::ostringstream config;
        config << "# tmoe-linux TigerVNC config — 自动生成\n"
                << "securitytypes=vncauth,tlsvnc\n"
                << "geometry=" << vnc_config_.resolution_w << "x" << vnc_config_.resolution_h << "\n"
                << "desktop=" << vnc_config_.desktop_name << "\n"
                << "depth=" << vnc_config_.pixel_depth << "\n"
                << "ZlibLevel=" << vnc_config_.zlib_level << "\n"
                << "localhost=" << (vnc_config_.localhost_only ? "yes" : "no") << "\n"
                << "CompareFB=" << (vnc_config_.compare_fb ? "1" : "0") << "\n"
                << "deferglyphs=16\n";

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
                << "export XDG_CURRENT_DESKTOP=XFCE\n"
                << "export XDG_RUNTIME_DIR=/tmp/runtime-${USER:-root}\n"
                << "export DESKTOP_SESSION=tmoe_linux\n"
                << "[ -d \"$XDG_RUNTIME_DIR\" ] || mkdir -p \"$XDG_RUNTIME_DIR\" 2>/dev/null\n"
                << "[ -w \"$XDG_RUNTIME_DIR\" ] || chmod 700 \"$XDG_RUNTIME_DIR\" 2>/dev/null\n\n"
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
        ExecResult result = Executor::passthrough(cmd + " > /tmp/tmoe_vnc_startup.log 2>&1");

        if (result.ok()) {
            Logger::ok(_f("gui.vnc.started",
                          std::to_string(vnc_config_.rfb_port) + " (display :" +
                          std::to_string(vnc_config_.display) + ")"));
            Logger::info("分辨率: " + std::to_string(vnc_config_.resolution_w) + "x" +
                         std::to_string(vnc_config_.resolution_h));
            Logger::info("连接地址: " + get_vnc_connection_uri());

            // 显示局域网地址
            std::string ips = get_local_ip_addresses();
            if (!ips.empty()) {
                Logger::info("局域网地址: " + ips);
            }

            // 写入 PID 文件
            write_vnc_pid_file(vnc_config_.display);

            return true;
        }

        Logger::error("VNC 启动失败，查看日志: /tmp/tmoe_vnc_startup.log");
        return false;
    }

    bool GUIManager::stop_vnc(int display) {
        Logger::step(_("gui.vnc.stop"));

        if (display <= 0) display = vnc_config_.display;

        // 1. 使用 vncserver -kill 清理 (TigerVNC)
        std::string cmd = "vncserver -kill :" + std::to_string(display) + " 2>/dev/null";
        Executor::passthrough(cmd);

        // 2. 基于 PID 文件精确停止
        remove_vnc_pid_file(display);

        // 3. 备用: pkill
        Executor::shell("pkill -f 'Xvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        Executor::shell("pkill -f 'Xtigervnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        Executor::shell("pkill -f 'Xtightvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");

        // 4. 清理锁文件
        Executor::shell("rm -f /tmp/.X" + std::to_string(display) + "-lock 2>/dev/null");
        Executor::shell("rm -f /tmp/.X11-unix/X" + std::to_string(display) + " 2>/dev/null");

        // 5. 停止 websockify (noVNC)
        Executor::shell("pkill -f 'websockify.*:" + std::to_string(vnc_config_.rfb_port) + "' 2>/dev/null || true");

        Logger::ok("VNC display :" + std::to_string(display) + " 已停止");
        return true;
    }

    bool GUIManager::start_x11vnc(int display) {
        Logger::step("启动 x11vnc 服务 (Xvfb)");

        int x11_display = (display > 0) ? display : 233;

        // 1. 启动 Xvfb
        std::string xvfb_cmd = build_xvfb_command(x11_display, 0, 0);
        Logger::debug("执行: " + xvfb_cmd);
        Executor::passthrough(xvfb_cmd);

        // 等待 Xvfb 就绪
        Executor::shell("sleep 2");

        // 2. 启动 x11vnc
        std::string x11vnc_cmd = build_x11vnc_command(x11_display);
        Logger::debug("执行: " + x11vnc_cmd);
        ExecResult result = Executor::passthrough(x11vnc_cmd);

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
            if (!Logger::confirm(_("gui.confirm_continue_install"))) {
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
            Executor::passthrough(cfg_.install_command + " " + pkg + " 2>/dev/null || true");
        }

        // 如果 apt 源没有 novnc，从 git 安装
        if (!fs::exists("/usr/share/novnc")) {
            Logger::info("从 GitHub 克隆 noVNC...");
            Executor::passthrough("git clone --depth=1 https://github.com/novnc/noVNC.git "
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
                                    "\" --inputbox \"" + std::string(_("gui.novnc_custom_port_input")) + "\" 10 40 \"36080\"";
            auto result = Executor::passthrough(input_cmd + " 2>&1");
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

        Executor::passthrough(cmd.str());
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
        Executor::passthrough("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null || true");

        Logger::ok("XRDP 安装完成，默认端口: 3389");
        return true;
    }

    bool GUIManager::configure_xrdp_session(std::string_view desktop) {
        DesktopInfo info = get_desktop_info(desktop);

        std::ostringstream wm;
        wm << "#!/bin/sh\n"
                << "# tmoe-linux XRDP startwm — 自动生成\n\n"
                << "# PulseAudio 桥接\n"
                << "if [ -z \"$PULSE_SERVER\" ] && [ -n \"$XRDP_PULSE_SINK\" ]; then\n"
                << "    export PULSE_SERVER=$XRDP_PULSE_SINK\n"
                << "fi\n\n"
                << "if command -v " << info.session_cmd1 << " >/dev/null 2>&1; then\n"
                << "    exec " << info.session_cmd1 << "\n"
                << "elif command -v " << info.session_cmd2 << " >/dev/null 2>&1; then\n"
                << "    exec " << info.session_cmd2 << "\n"
                << "else\n"
                << "    exec xterm\n"
                << "fi\n";

        return write_file("/etc/xrdp/startwm.sh", wm.str()) &&
               Executor::shell("chmod +x /etc/xrdp/startwm.sh").ok();
    }

    bool GUIManager::start_xrdp() {
        Logger::step("启动 XRDP 服务...");
        if (Executor::passthrough("service xrdp start 2>/dev/null || systemctl start xrdp 2>/dev/null").ok()) {
            Logger::ok("XRDP 已启动 — 端口 3389");
            return true;
        }
        // 备用: 直接启动 xrdp 守护进程
        if (Executor::passthrough("xrdp 2>/dev/null &").ok()) {
            Logger::ok("XRDP 已启动 (直接模式)");
            return true;
        }
        Logger::error("XRDP 启动失败");
        return false;
    }

    bool GUIManager::stop_xrdp() {
        Logger::step("停止 XRDP...");
        Executor::passthrough("service xrdp stop 2>/dev/null || systemctl stop xrdp 2>/dev/null || "
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
                                    "\" --inputbox \"" + std::string(_("gui.xsdl_custom_input")) + "\" 10 40 \"127.0.0.1:0\"";
            auto result = Executor::passthrough(input_cmd + " 2>&1");
            if (!result.stdout_data.empty()) {
                display = result.stdout_data;
                while (!display.empty() && (display.back() == '\n' || display.back() == '\r'))
                    display.pop_back();
            }
        }

        // DISPLAY should be set via C++ setenv instead of a standalone subshell export
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
        Executor::passthrough(cmd);

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
        Executor::passthrough(cmd);
        Executor::shell("sleep 1");
        // DISPLAY should be set via C++ setenv instead of a standalone subshell export

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
        if (Executor::passthrough("service dbus start 2>/dev/null").ok()) return true;
        if (Executor::passthrough("systemctl start dbus 2>/dev/null").ok()) return true;

        // 直接启动 dbus-daemon
        Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
        if (Executor::passthrough("dbus-daemon --system --fork 2>/dev/null").ok()) return true;

        Logger::debug("使用 dbus-launch 启动会话 dbus");
        return Executor::passthrough("dbus-launch --exit-with-session 2>/dev/null &").ok();
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

        return Executor::passthrough(cfg_.install_command + " " + pkg_name + " 2>/dev/null || true").ok();
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

        return Executor::passthrough(cmd + " >/dev/null 2>&1").ok();
    }

    // ═══════════════════════════════════════════════════════════════
    // TUI 交互式菜单
    // ═══════════════════════════════════════════════════════════════

    void GUIManager::run_gui_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.main_title")) + "\""
                                   " --menu \"" + std::string(_("gui.main_menu_prompt")) + vnc_config_.server +
                                   " — 端口 " + std::to_string(vnc_config_.rfb_port) +
                                   " — 分辨率 " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.install_de")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.install_vnc")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.start_vnc")) + "\" "
                                   "\"4\" \"" + std::string(_("gui.stop_vnc")) + "\" "
                                   "\"5\" \"" + std::string(_("gui.novnc")) + "\" "
                                   "\"6\" \"" + std::string(_("gui.xrdp")) + "\" "
                                   "\"7\" \"" + std::string(_("gui.xsdl")) + "\" "
                                   "\"8\" \"" + std::string(_("gui.vnc_config")) + "\" "
                                   "\"9\" \"" + std::string(_("gui.beautify")) + "\" "
                                   "\"A\" \"" + std::string(_("gui.install_fonts")) + "\" "
                                   "\"B\" \"" + std::string(_("gui.install_fcitx")) + "\" "
                                   "\"C\" \"" + std::string(_("gui.install_chromium")) + "\" "
                                   "\"D\" \"" + std::string(_("gui.deploy_scripts")) + "\" "
                                   "\"E\" \"" + std::string(_("gui.fix_perms")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back_upper")) + "\"";

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
                if (Logger::confirm(_("gui.confirm_stop_vnc"))) {
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
            } else if (choice == "A") {
                install_fonts();
                install_iosevka_font();
                Logger::press_enter();
            } else if (choice == "B") {
                install_fcitx();
                Logger::press_enter();
            } else if (choice == "C") {
                install_chromium();
                Logger::press_enter();
            } else if (choice == "D") {
                deploy_startup_scripts();
                Logger::press_enter();
            } else if (choice == "E") {
                fix_vnc_permissions();
                Logger::press_enter();
            }
        }
    }

    void GUIManager::run_vnc_config_menu() {
        while (true) {
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.vnc_config_title")) +
                                   "\" --menu \"" + std::string(_("gui.vnc_config_prompt")) +
                                   vnc_config_.server + " — 端口 " + std::to_string(vnc_config_.rfb_port) +
                                   " — " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.vnc_password")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.vnc_switch_server")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.vnc_resolution")) + " " + std::to_string(vnc_config_.resolution_w) + "x" +
                                   std::to_string(vnc_config_.resolution_h) + ")\" "
                                   "\"4\" \"" + std::string(_("gui.vnc_port")) + " " + std::to_string(vnc_config_.rfb_port) + ")\" "
                                   "\"5\" \"" + std::string(_("gui.vnc_depth")) + " " + std::to_string(vnc_config_.pixel_depth) + ")\" "
                                   "\"6\" \"" + std::string(_("gui.vnc_zlib")) + " " + std::to_string(vnc_config_.zlib_level) + ")\" "
                                   "\"7\" \"" + std::string(_("gui.vnc_pulseaudio")) + "\" "
                                   "\"8\" \"" + std::string(_("gui.vnc_fix_dbus")) + "\" "
                                   "\"9\" \"" + std::string(_("gui.vnc_dbus_status")) + "\" "
                                   "\"10\" \"" + std::string(_("gui.vnc_stop_dbus")) + "\" "
                                   "\"11\" \"" + std::string(_("gui.vnc_comparefb")) + " " +
                                   std::string(vnc_config_.compare_fb ? "ON" : "OFF") + ")\" "
                                   "\"12\" \"" + std::string(_("gui.vnc_localhost")) + " " +
                                   std::string(vnc_config_.localhost_only ? "ON" : "OFF") + ")\" "
                                   "\"13\" \"" + std::string(_("gui.vnc_kill_all")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

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
                        vnc_config_.resolution_w = std::stoi(res.substr(0, xpos));
                        vnc_config_.resolution_h = std::stoi(res.substr(xpos + 1));
                        configure_vnc_defaults();
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
                    vnc_config_.display = p - 5900;
                    vnc_config_.rfb_port = p;
                }
            } else if (choice == "5") {
                std::string depth_cmd = cfg_.tui_bin +
                                        " --title \"" + std::string(_("gui.depth_title")) +
                                        "\" --menu \"" + std::string(_("gui.depth_prompt")) + "\" 0 0 0 "
                                        "\"24\" \"" + std::string(_("gui.depth_24")) + "\" "
                                        "\"16\" \"" + std::string(_("gui.depth_16")) + "\"";
                std::string depth = Executor::tui_select(depth_cmd);
                if (!depth.empty()) vnc_config_.pixel_depth = std::stoi(depth);
            } else if (choice == "6") {
                std::string zlib_cmd = cfg_.tui_bin +
                                       " --title \"" + std::string(_("gui.zlib_title")) +
                                       "\" --menu \"" + std::string(_("gui.zlib_prompt")) + "\" 0 0 0 "
                                       "\"0\" \"" + std::string(_("gui.zlib_0")) + "\" "
                                       "\"3\" \"" + std::string(_("gui.zlib_3")) + "\" "
                                       "\"6\" \"" + std::string(_("gui.zlib_6")) + "\" "
                                       "\"9\" \"" + std::string(_("gui.zlib_9")) + "\"";
                std::string zlib = Executor::tui_select(zlib_cmd);
                if (!zlib.empty()) vnc_config_.zlib_level = std::stoi(zlib);
            } else if (choice == "7") {
                start_pulseaudio_bridge();
            } else if (choice == "8") {
                fix_vnc_dbus();
            } else if (choice == "9") {
                show_dbus_status();
            } else if (choice == "10") {
                if (Logger::confirm(_("gui.confirm_stop_dbus"))) {
                    stop_dbus_daemon();
                }
            } else if (choice == "11") {
                vnc_config_.compare_fb = !vnc_config_.compare_fb;
                configure_vnc_defaults();
                Logger::ok("CompareFB 已" + std::string(vnc_config_.compare_fb ? "启用" : "禁用"));
            } else if (choice == "12") {
                vnc_config_.localhost_only = !vnc_config_.localhost_only;
                configure_vnc_defaults();
                Logger::ok("localhost 限制已" + std::string(vnc_config_.localhost_only ? "启用" : "禁用"));
            } else if (choice == "13") {
                if (Logger::confirm(_("gui.confirm_kill_vnc"))) {
                    kill_all_vnc();
                }
            }
            Logger::press_enter();
        }
    }

    void GUIManager::run_desktop_install_menu() {
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"" + std::string(_("gui.de_install_title")) + "\""
                               " --menu \"" + std::string(_("gui.de_install_prompt")) + "\" 0 0 0 ";

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
        menu_cmd += "\"wm\" \"" + std::string(_("gui.de_install_wm")) + "\" ";
        menu_cmd += "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice == "0" || choice.empty()) return;

        if (choice == "wm") {
            // 窗口管理器子菜单
            std::string wm_menu = cfg_.tui_bin +
                                  " --title \"" + std::string(_("gui.wm_title")) +
                                  "\" --menu \"" + std::string(_("gui.wm_prompt")) + "\" 0 0 0 ";
            int idx = 1;
            for (const auto &name: list_window_managers()) {
                wm_menu += "\"" + std::to_string(idx++) + "\" \"" + name + "\" ";
            }
            wm_menu += "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
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
                                   " --title \"" + std::string(_("gui.remote_title")) +
                                   "\" --menu \"" + std::string(_("gui.remote_prompt")) + "\" 0 0 0 "
                                   "\"1\" \"" + std::string(_("gui.remote_novnc")) + "\" "
                                   "\"2\" \"" + std::string(_("gui.remote_x11vnc")) + "\" "
                                   "\"3\" \"" + std::string(_("gui.remote_xrdp")) + "\" "
                                   "\"4\" \"" + std::string(_("gui.remote_xsdl")) + "\" "
                                   "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                install_novnc();
                configure_novnc();
                start_novnc();
            } else if (choice == "2") {
                if (Logger::confirm(_("gui.confirm_start_x11vnc"))) {
                    stop_vnc();
                    start_x11vnc();
                }
            } else if (choice == "3") {
                // XRDP 子菜单
                while (true) {
                    std::string xrdp_menu = cfg_.tui_bin +
                                            " --title \"" + std::string(_("gui.xrdp_title")) +
                                            "\" --menu \"" + std::string(_("gui.xrdp_prompt")) + "\" 0 0 0 "
                                            "\"1\" \"" + std::string(_("gui.xrdp_install")) + "\" "
                                            "\"2\" \"" + std::string(_("gui.xrdp_start")) + "\" "
                                            "\"3\" \"" + std::string(_("gui.xrdp_stop")) + "\" "
                                            "\"4\" \"" + std::string(_("gui.xrdp_restart")) + "\" "
                                            "\"5\" \"" + std::string(_("gui.xrdp_change_port")) + "\" "
                                            "\"6\" \"" + std::string(_("gui.xrdp_remove")) + "\" "
                                            "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";
                    std::string xchoice = Executor::tui_select(xrdp_menu);
                    if (xchoice == "0" || xchoice.empty()) break;
                    if (xchoice == "1") install_xrdp();
                    else if (xchoice == "2") start_xrdp();
                    else if (xchoice == "3") stop_xrdp();
                    else if (xchoice == "4") restart_xrdp();
                    else if (xchoice == "5") {
                        std::string port_cmd = cfg_.tui_bin +
                                               " --title \"" + std::string(_("gui.xrdp_port_title")) +
                                               "\" --inputbox \"" + std::string(_("gui.xrdp_port_input")) + "\" 10 40 \"3389\"";
                        auto result = Executor::passthrough(port_cmd + " 2>&1");
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

    bool GUIManager::auto_install_gui(std::string_view desktop) {
        Logger::info("自动安装 GUI 模式: " + std::string(desktop));

        // 1. 安装字体
        install_fonts();

        // 2. 安装 VNC 服务端
        if (!install_vnc_server()) {
            Logger::error("VNC 服务端安装失败");
            return false;
        }

        // 3. 安装桌面环境
        if (!install_desktop(desktop)) {
            Logger::error("桌面环境安装失败");
            return false;
        }

        // 4. 配置 VNC 密码
        if (!configure_vnc_password("tmoe123")) {
            Logger::warn("VNC 密码设置失败，使用无密码模式");
        }

        // 5. 根据桌面推荐服务端
        std::string d(desktop);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (d == "kde" || d == "gnome" || d == "cinnamon" || d == "dde") {
            vnc_config_.server = "tiger";
        }

        // 6. HiDPI 自动适配
        detect_and_configure_hidpi(desktop);

        // 7. 安装输入法
        install_fcitx();

        // 8. 修复权限
        fix_vnc_permissions();

        // 9. 部署启动脚本
        deploy_startup_scripts();

        // 10. 启动 VNC
        bool ok = start_vnc();

        if (ok) {
            Logger::ok("🎉 GUI 自动安装完成！");
            Logger::info("VNC 地址: " + get_vnc_connection_uri());
            Logger::info("登录密码: tmoe123 (请及时修改)");
        }

        return ok;
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

    std::string GUIManager::get_local_ip_addresses() const {
        std::ostringstream ips;
        // IPv4
        auto v4 = Executor::shell("ip -4 addr show scope global | grep inet | awk '{print $2}' | cut -d/ -f1 2>/dev/null");
        if (v4.ok() && !v4.stdout_data.empty()) {
            std::string data = v4.stdout_data;
            std::istringstream iss(data);
            std::string line;
            while (std::getline(iss, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
                while (!line.empty() && (line.front() == ' ')) line.erase(0, 1);
                if (!line.empty()) ips << "vnc://" << line << ":" << vnc_config_.rfb_port << "  ";
            }
        }
        if (ips.str().empty()) {
            auto v4_host = Executor::shell("hostname -I 2>/dev/null | awk '{for(i=1;i<=NF;i++){if($i ~ /^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$/ && $i != \"127.0.0.1\"){print $i;exit}}}'");
            if (v4_host.ok() && !v4_host.stdout_data.empty()) {
                std::string ip = v4_host.stdout_data;
                while (!ip.empty() && (ip.back() == '\r' || ip.back() == '\n' || ip.back() == ' ')) ip.pop_back();
                if (!ip.empty()) ips << "vnc://" << ip << ":" << vnc_config_.rfb_port;
            }
        }
        return ips.str();
    }

    // ═══════════════════════════════════════════════════════════════
    // VNC PID 管理
    // ═══════════════════════════════════════════════════════════════

    void GUIManager::write_vnc_pid_file(int display) const {
        std::ostringstream cmd;
        cmd << "pgrep -f 'X(vnc|tigervnc|tightvnc).*:" << display << "' | head -1 > "
                << vnc_config_.vnc_pid_file.string() << " 2>/dev/null";
        Executor::shell(cmd.str());
        // 也写入 x.pid (TigerVNC 兼容)
        Executor::shell("pgrep -f 'X(tigervnc|tightvnc).*:" + std::to_string(display) + "' | head -1 > "
                + vnc_config_.x_pid_file.string() + " 2>/dev/null || true");
    }

    void GUIManager::remove_vnc_pid_file(int display) const {
        // 基于 PID 文件杀进程
        if (fs::exists(vnc_config_.vnc_pid_file)) {
            auto pid_data = read_file(vnc_config_.vnc_pid_file);
            if (!pid_data.empty()) {
                std::string mypid = pid_data;
                while (!mypid.empty() && (mypid.back() == '\n' || mypid.back() == '\r')) mypid.pop_back();
                if (!mypid.empty())
                    Executor::passthrough("kill " + mypid + " 2>/dev/null || true");
            }
            fs::remove(vnc_config_.vnc_pid_file);
        }
        if (fs::exists(vnc_config_.x_pid_file)) {
            auto pid_data = read_file(vnc_config_.x_pid_file);
            if (!pid_data.empty()) {
                std::string mypid = pid_data;
                while (!mypid.empty() && (mypid.back() == '\n' || mypid.back() == '\r')) mypid.pop_back();
                if (!mypid.empty())
                    Executor::passthrough("kill " + mypid + " 2>/dev/null || true");
            }
            fs::remove(vnc_config_.x_pid_file);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 字体管理
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_fonts() {
        Logger::step(_("gui.font.install"));

        // 检测发行版并安装对应字体包
        bool has_apt = Executor::has("apt-get");
        bool has_pacman = Executor::has("pacman");

        if (has_apt) {
            Logger::info("安装中文字体 (fonts-noto-cjk)...");
            Executor::passthrough(cfg_.install_command +
                " fonts-noto-cjk fonts-noto-color-emoji 2>/dev/null || true");
        } else if (has_pacman) {
            Logger::info("安装中文字体 (noto-fonts-cjk)...");
            Executor::passthrough(cfg_.install_command +
                " noto-fonts-cjk noto-fonts-emoji 2>/dev/null || true");
        } else {
            // 通用尝试
            Executor::passthrough(cfg_.install_command +
                " fonts-noto-cjk 2>/dev/null || "
                + cfg_.install_command + " wqy-microhei 2>/dev/null || true");
        }

        // 刷新字体缓存
        Executor::passthrough("fc-cache -fv 2>/dev/null || true");
        Logger::ok(_("gui.font.install_ok"));
        return true;
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
        if (!cfg_.is_termux) return false;

        int w = 0, h = 0;
        if (!detect_android_resolution(w, h)) return false;

        // 检测 HiDPI: 高度 >= 2340 或宽度 >= 1440 且 DPI 高
        int original_w = 0, original_h = 0;
        auto result = Executor::shell("wm size 2>/dev/null | grep -oP '[0-9]+x[0-9]+'");
        if (result.ok()) {
            auto xpos = result.stdout_data.find('x');
            if (xpos != std::string::npos) {
                original_w = std::stoi(result.stdout_data.substr(0, xpos));
                original_h = std::stoi(result.stdout_data.substr(xpos + 1));
            }
        }

        int max_dim = std::max(original_w, original_h);
        if (max_dim >= 2340) {
            Logger::info("检测到 HiDPI 屏幕 (" + std::to_string(max_dim) + "px), 自动设置缩放...");

            // 设置 XFCE 窗口缩放
            Executor::shell("xfconf-query -c xsettings -p /Gdk/WindowScalingFactor -s 2 2>/dev/null || true");
            Executor::shell("xfconf-query -c xfwm4 -p /general/theme -s Default-xhdpi 2>/dev/null || true");

            vnc_config_.resolution_w = (max_dim > 1440) ? 1920 : 1440;
            vnc_config_.resolution_h = (max_dim > 1440) ? 1080 : std::min(original_h * 720 / std::max(original_w, 1), 810);

            Logger::info("VNC 分辨率自动调整为: " +
                         std::to_string(vnc_config_.resolution_w) + "x" +
                         std::to_string(vnc_config_.resolution_h));
        }

        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 输入法与浏览器
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::install_fcitx() {
        Logger::step("安装 fcitx 中文输入法...");

        // fcitx + 拼音 + 配置工具
        std::vector<std::string> pkgs = {
            "fcitx", "fcitx-pinyin", "fcitx-config-gtk", "fcitx-frontend-gtk2",
            "fcitx-frontend-gtk3", "fcitx-frontend-qt5"
        };

        if (!install_packages(pkgs)) {
            Logger::warn("部分 fcitx 组件安装失败");
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
        append_file(fs::path(bashrc), env_block);

        // 创建 fcitx 自启动
        std::string autostart_dir = home + "/.config/autostart";
        Executor::shell("mkdir -p " + autostart_dir);
        std::string fcitx_desktop = autostart_dir + "/fcitx.desktop";
        std::string desktop_content =
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Fcitx\n"
            "Exec=fcitx-autostart\n"
            "X-GNOME-Autostart-enabled=true\n";
        write_file(fs::path(fcitx_desktop), desktop_content);

        Logger::ok("fcitx 中文输入法安装完成，重新登录后生效");
        return true;
    }

    bool GUIManager::install_chromium() {
        Logger::step("安装 Chromium 浏览器...");

        if (Executor::has("chromium") || Executor::has("chromium-browser")) {
            Logger::ok("Chromium 已安装");
            return true;
        }

        // 尝试多种方式安装
        if (Executor::passthrough(cfg_.install_command + " chromium-browser 2>/dev/null").ok()) {
            Logger::ok("Chromium 安装完成");
            return true;
        }
        if (Executor::passthrough(cfg_.install_command + " chromium 2>/dev/null").ok()) {
            Logger::ok("Chromium 安装完成");
            return true;
        }
        if (Executor::passthrough(cfg_.install_command + " google-chrome-stable 2>/dev/null").ok()) {
            Logger::ok("Chrome 安装完成 (备选)");
            return true;
        }

        Logger::warn("Chromium 安装失败，请手动安装");
        return false;
    }

    // ═══════════════════════════════════════════════════════════════
    // 权限与文件管理
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::fix_vnc_permissions() {
        Logger::step("修复 VNC 目录权限...");
        Executor::shell("chown -R $(id -un):$(id -gn) " +
            vnc_config_.vnc_home_dir.string() + " 2>/dev/null || true");
        Executor::shell("chmod -R 700 " +
            vnc_config_.vnc_home_dir.string() + " 2>/dev/null || true");
        if (fs::exists(vnc_config_.passwd_file))
            Executor::shell("chmod 600 " + vnc_config_.passwd_file.string() + " 2>/dev/null || true");
        Logger::ok("VNC 权限已修复");
        return true;
    }

    bool GUIManager::deploy_startup_scripts() {
        Logger::step("部署启动脚本到 /usr/local/bin...");

        // startvnc 脚本
        std::string startvnc =
            "#!/bin/bash\n# tmoe-linux startvnc — 自动生成\n"
            "tmoe gui startvnc\n";
        write_file("/usr/local/bin/startvnc", startvnc);
        Executor::shell("chmod +x /usr/local/bin/startvnc");

        // startxsdl 脚本
        std::string startxsdl =
            "#!/bin/bash\n# tmoe-linux startxsdl — 自动生成\n"
            "tmoe gui xsdl\n";
        write_file("/usr/local/bin/startxsdl", startxsdl);
        Executor::shell("chmod +x /usr/local/bin/startxsdl");

        // stopvnc 脚本
        std::string stopvnc =
            "#!/bin/bash\n# tmoe-linux stopvnc — 自动生成\n"
            "tmoe gui stop\n";
        write_file("/usr/local/bin/stopvnc", stopvnc);
        Executor::shell("chmod +x /usr/local/bin/stopvnc");

        Logger::ok("启动脚本已部署: startvnc, startxsdl, stopvnc");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // noVNC 增强
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

    // ═══════════════════════════════════════════════════════════════
    // D-Bus 增强
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::stop_dbus_daemon() {
        Logger::debug("停止 D-Bus 守护进程...");

        // 从 PID 文件精确停止
        if (fs::exists("/run/dbus/pid")) {
            auto pid_data = read_file("/run/dbus/pid");
            if (!pid_data.empty()) {
                Executor::passthrough("kill " + pid_data + " 2>/dev/null || true");
            }
            fs::remove("/run/dbus/pid");
        }
        if (fs::exists("/var/run/dbus/pid")) {
            auto pid_data = read_file("/var/run/dbus/pid");
            if (!pid_data.empty()) {
                Executor::passthrough("kill " + pid_data + " 2>/dev/null || true");
            }
            fs::remove("/var/run/dbus/pid");
        }

        // 标准停止
        Executor::passthrough("service dbus stop 2>/dev/null || systemctl stop dbus 2>/dev/null || "
            "pkill dbus-daemon 2>/dev/null || true");
        Executor::shell("pkill dbus-launch 2>/dev/null || true");

        Logger::ok("D-Bus 守护进程已停止");
        return true;
    }

    void GUIManager::show_dbus_status() const {
        if (fs::exists("/run/dbus/pid") || fs::exists("/var/run/dbus/pid")) {
            Logger::info("D-Bus 守护进程: 运行中 ✓");
        } else {
            auto result = Executor::shell("pgrep dbus-daemon 2>/dev/null");
            if (result.ok() && !result.stdout_data.empty()) {
                Logger::info("D-Bus 守护进程: 运行中 ✓ (PID: " + result.stdout_data + ")");
            } else {
                Logger::info("D-Bus 守护进程: 未运行 ✗");
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面美化扩展
    // ═══════════════════════════════════════════════════════════════

    bool GUIManager::download_wallpaper(std::string_view source) {
        Logger::step("下载壁纸: " + std::string(source));

        std::string wallpaper_dir = "/usr/share/backgrounds/tmoe";
        Executor::shell("mkdir -p " + wallpaper_dir);

        std::string url;
        std::string filename;
        std::string src_lower(source);
        std::transform(src_lower.begin(), src_lower.end(), src_lower.begin(), ::tolower);

        if (src_lower == "debian" || src_lower == "gnome") {
            install_packages({"gnome-backgrounds"});
            Logger::ok("GNOME 壁纸包已安装");
            return true;
        } else if (src_lower == "xfce" || src_lower == "xubuntu") {
            url = "https://gitlab.xfce.org/artwork/xfce4-artwork/-/raw/master/backgrounds/xfce-stripes.png";
            filename = "xfce-stripes.png";
        } else if (src_lower == "mate" || src_lower == "ubuntu-mate") {
            install_packages({"ubuntu-mate-wallpapers"});
            Logger::ok("Ubuntu MATE 壁纸包已安装");
            return true;
        } else if (src_lower == "deepin") {
            install_packages({"deepin-wallpapers"});
            Logger::ok("Deepin 壁纸包已安装");
            return true;
        } else if (src_lower == "kde") {
            install_packages({"plasma-workspace-wallpapers"});
            Logger::ok("KDE 壁纸包已安装");
            return true;
        } else {
            Logger::info("未知壁纸源: " + std::string(source) + "，跳过下载");
            return false;
        }

        if (!url.empty()) {
            Executor::passthrough("wget -q '" + url + "' -O " + wallpaper_dir + "/" + filename +
                " 2>/dev/null || curl -sL '" + url + "' -o " + wallpaper_dir + "/" + filename + " 2>/dev/null");
            set_wallpaper(wallpaper_dir + "/" + filename);
        }

        Logger::ok("壁纸下载完成");
        return true;
    }

    bool GUIManager::install_conky() {
        Logger::step("安装 Conky 系统监控...");
        if (!install_packages({"conky", "conky-all"})) {
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
        write_file(fs::path(conky_dir + "/conky.conf"), conky_conf);

        // 创建 autostart
        std::string autostart = home + "/.config/autostart/conky.desktop";
        std::string desktop_entry =
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Conky\n"
            "Exec=conky -c " + conky_dir + "/conky.conf\n"
            "X-GNOME-Autostart-enabled=true\n";
        write_file(fs::path(autostart), desktop_entry);

        Logger::ok("Conky 安装完成");
        return true;
    }

    bool GUIManager::install_compiz() {
        Logger::step("安装 Compiz 窗口特效...");
        std::vector<std::string> pkgs = {
            "compiz", "compiz-core", "compiz-plugins",
            "compiz-plugins-default", "compiz-plugins-extra",
            "emerald", "emerald-themes", "compizconfig-settings-manager"
        };

        if (!install_packages(pkgs)) {
            Logger::warn("部分 Compiz 组件安装失败，尝试核心包...");
            install_packages({"compiz", "compiz-core", "compiz-plugins"});
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

        std::string pkg_name;
        if (name_lower.find("breeze") != std::string::npos) pkg_name = "breeze-cursor-theme";
        else if (name_lower.find("chameleon") != std::string::npos) pkg_name = "chameleon-cursor-theme";
        else pkg_name = std::string(theme) + "-cursor-theme";

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
        return write_file(panel_dir / "xfce4-panel.xml", panel_xml);
    }

    std::string GUIManager::generate_xfce_panel_xml() const {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-panel" version="1.0">
  <property name="configver" type="int" value="2"/>
  <property name="panels" type="array">
    <value type="int" value="1"/>
    <property name="panel-1" type="empty">
      <property name="position" type="string" value="p=8;x=0;y=0"/>
      <property name="length" type="uint" value="100"/>
      <property name="position-locked" type="bool" value="true"/>
      <property name="size" type="uint" value="38"/>
      <property name="plugin-ids" type="array">
        <value type="int" value="1"/>
        <value type="int" value="2"/>
        <value type="int" value="3"/>
        <value type="int" value="4"/>
        <value type="int" value="5"/>
        <value type="int" value="6"/>
        <value type="int" value="7"/>
        <value type="int" value="8"/>
        <value type="int" value="9"/>
      </property>
    </property>
  </property>
  <property name="plugins" type="empty">
    <property name="plugin-1" type="string" value="applicationsmenu"/>
    <property name="plugin-2" type="string" value="tasklist"/>
    <property name="plugin-3" type="string" value="separator"/>
    <property name="plugin-4" type="string" value="clock">
      <property name="digital-layout" type="uint" value="2"/>
      <property name="mode" type="uint" value="2"/>
    </property>
    <property name="plugin-5" type="string" value="separator"/>
    <property name="plugin-6" type="string" value="systray"/>
    <property name="plugin-7" type="string" value="pulseaudio"/>
    <property name="plugin-8" type="string" value="notification-plugin"/>
    <property name="plugin-9" type="string" value="actions"/>
  </property>
</channel>
)";
    }

    // ═══════════════════════════════════════════════════════════════
    // 美化菜单扩展
    // ═══════════════════════════════════════════════════════════════

    // run_beautification_menu 被扩展以包含新选项
} // namespace tmoe::domain
