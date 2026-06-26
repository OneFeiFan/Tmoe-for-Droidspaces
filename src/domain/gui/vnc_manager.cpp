#include "vnc_manager.h"

#include "domain/system/package_manager.h"

namespace tmoe::domain {
    // ================================================================
    // VncConfig 实现
    // ================================================================

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

    // ================================================================
    // VncManager 构造与初始化
    // ================================================================

    VncManager::VncManager(const TmoeConfig &cfg)
        : cfg_(cfg) {
        vnc_config_.init_defaults();
    }

    // ================================================================
    // 私有辅助方法
    // ================================================================

    bool VncManager::ensure_vnc_home_dir() const {
        if (fs::exists(vnc_config_.vnc_home_dir)) {
            return true;
        }
        std::error_code ec;
        if (!fs::create_directories(vnc_config_.vnc_home_dir, ec)) {
            Logger::error(_f("gui.vnc.create_home_failed",
                             vnc_config_.vnc_home_dir.string() + ": " + ec.message()));
            return false;
        }
        return true;
    }

    bool VncManager::run_shell_command(const std::string &cmd, bool log_on_fail) const {
        auto res = Executor::passthrough(cmd);
        if (!res.ok() && log_on_fail) {
            Logger::debug("Command failed: " + cmd + " (exit " + std::to_string(res.exit_code) + ")");
        }
        return res.ok();
    }

    bool VncManager::write_file_content(const fs::path &path, std::string_view content) const {
        return SystemHelper::write_file(path, content);
    }

    // ================================================================
    // VNC 服务端安装与配置
    // ================================================================

    bool VncManager::install_vnc_server() {
        Logger::step(_("gui.vnc.install"));

        // 使用 PackageManager 统一安装
        using namespace tmoe::domain;
        auto family = PackageManager::detect_distro_family();

        // 必要包列表（部分分发版可能不同，但 PackageManager 会处理）
        std::vector<std::string> pkgs = {
            "tigervnc-standalone-server",
            "tigervnc-viewer",
            "xfonts-100dpi",
            "xfonts-75dpi",
            "xfonts-scalable",
            "x11vnc",
            "xvfb"
        };

        // 移除可能冲突的包（若有）
        if (family == DistroFamily::Debian) {
            // 对 Debian 类发行版，先确保移除 tightvncserver 以免冲突
            PackageManager::remove("tightvncserver", family);
        }

        bool ok = PackageManager::install(pkgs, family);
        if (!ok) {
            Logger::warn(_("gui.vnc.install_partial"));
        }

        // 检查 Xvnc 命令
        return check_xvnc_command();
    }

    bool VncManager::check_xvnc_command() {
        if (Executor::has("Xvnc")) {
            Logger::ok(_("gui.vnc.xvnc_ok"));
            return true;
        }
        if (Executor::has("Xtigervnc")) {
            Logger::ok(_("gui.vnc.xtigervnc_ok"));
            return true;
        }
        if (Executor::has("Xtightvnc")) {
            Logger::ok(_("gui.vnc.xtightvnc_ok"));
            return true;
        }

        Logger::warn(_("gui.vnc.server_not_found"));

        // 按发行版安装对应包
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        std::string dep;
        switch (family) {
            case DistroFamily::Debian: dep = "tigervnc-standalone-server";
                break;
            case DistroFamily::RedHat: dep = "tigervnc-server";
                break;
            case DistroFamily::Arch: dep = "tigervnc";
                break;
            case DistroFamily::Void_: dep = "tigervnc";
                break;
            case DistroFamily::Suse: dep = "tigervnc-x11vnc";
                break;
            default: dep = "tigervnc-standalone-server";
                break;
        }

        // 尝试安装
        PackageManager::install(dep, family);

        // Arch 字体补充检测
        if (family == DistroFamily::Arch) {
            if (!fs::exists("/usr/share/fonts/noto-cjk"))
                PackageManager::install("noto-fonts-cjk", family);
            if (!fs::exists("/usr/share/fonts/noto/NotoColorEmoji.ttf"))
                PackageManager::install("noto-fonts-emoji", family);
        }

        // 再次检查是否成功
        return Executor::has("Xvnc") || Executor::has("Xtigervnc") || Executor::has("Xtightvnc");
    }

    // ---------- 内部安装辅助 (部分保留，但使用 PackageManager) ----------

    void VncManager::tiger_vnc_variable() {
        vnc_config_.server = "tiger";
        vnc_config_.server_bin = "tigervnc";
        vnc_config_.dep_viewer = "tigervnc-viewer";
        vnc_config_.dep_server = "tigervnc-standalone-server";
    }

    void VncManager::tight_vnc_variable() {
        vnc_config_.server = "tight";
        vnc_config_.server_bin = "tightvnc";
        vnc_config_.dep_viewer = "tigervnc-viewer xfonts-100dpi xfonts-75dpi xfonts-scalable";
        vnc_config_.dep_server = "tightvncserver";
    }

    void VncManager::debian_remove_vnc_server() {
        std::string current = vnc_config_.server_bin;
        if (current == "tigervnc") current = "tigervnc-standalone-server";
        Logger::info(_f("gui.vnc.removing_server", current));
        PackageManager::remove(current, DistroFamily::Debian);
    }

    void VncManager::debian_install_vnc_server() {
        Logger::step(_("gui.vnc.debian_install"));

        auto family = DistroFamily::Debian;

        if (!Executor::has("Xtigervnc")) {
            PackageManager::install("tigervnc-standalone-server", family);
            PackageManager::install("tigervnc-viewer", family);
        }
        if (!Executor::has("Xtigervnc")) {
            // Fallback to vnc4server
            PackageManager::install("vnc4server", family);
        }

        if (!Executor::has("Xtightvnc")) {
            PackageManager::install("tightvncserver", family);
            // Fix postinst if needed (tightvnc postinst may fail)
            run_shell_command("sed -i -E 's@(configure)@pre\\1@' "
                "/var/lib/dpkg/info/tightvncserver.postinst 2>/dev/null || true");
            run_shell_command("dpkg --configure -a 2>/dev/null || true");
        }

        // xfonts
        if (!fs::exists("/usr/share/fonts/X11/100dpi/timR24.pcf.gz")) {
            PackageManager::install("xfonts-100dpi", family);
        }
        if (!fs::exists("/usr/share/fonts/X11/75dpi/term14.pcf.gz")) {
            PackageManager::install("xfonts-75dpi", family);
        }
        if (!fs::exists("/usr/share/fonts/X11/Type1/c0419bt_.afm")) {
            PackageManager::install("xfonts-scalable", family);
        }

        // Speedo 字体软链接
        if (fs::exists("/usr/share/fonts/X11/Type1") && !fs::exists("/usr/share/fonts/X11/Speedo")) {
            run_shell_command("ln -svf /usr/share/fonts/X11/Type1 /usr/share/fonts/X11/Speedo 2>/dev/null || true");
        }

        // 写回 VNC_SERVER 到 startvnc
        if (!vnc_config_.server.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i").add_flag("-E")
                              .add_arg("s@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server + "@")
                              .add_arg("/usr/local/bin/startvnc").add_raw("2>/dev/null || true").build_string());
        }
    }

    std::string VncManager::grep_tiger_vnc_deb_file(const std::string &latest_deb_repo,
                                                    const std::string &grep_name_01,
                                                    const std::string &grep_name_02) {
        // 获取架构
        std::string arch;
        auto arch_res = Executor::shell("dpkg --print-architecture 2>/dev/null || uname -m");
        if (arch_res.ok()) {
            arch = arch_res.stdout_data;
            while (!arch.empty() && (arch.back() == '\n' || arch.back() == '\r')) arch.pop_back();
        } else {
            arch = "amd64";
        }

        auto result = Executor::shell("curl -L '" + latest_deb_repo + "' 2>/dev/null | grep '\\.deb' | "
                                      "grep '" + arch + "' | grep '" + grep_name_01 + "' | grep '" + grep_name_02 +
                                      "' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2");
        if (!result.ok() || result.stdout_data.empty()) return "";
        std::string version = result.stdout_data;
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();
        return latest_deb_repo + version;
    }

    void VncManager::ubuntu_install_tiger_vnc_server() {
        Logger::step(_("gui.vnc.ubuntu_focal_install"));

        run_shell_command("apt-mark unhold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
        debian_install_vnc_server();

        const std::string deb_repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/t/tigervnc/";
        const std::string tmp_dir = "/tmp/.TIGER_VNC_TEMP_FOLDER";
        run_shell_command(CommandBuilder("mkdir").add_flag("-p").add_arg(tmp_dir).build_string());

        auto download_deb = [&](const std::string &url, const std::string &output) {
            if (url.empty()) return;
            run_shell_command("cd " + tmp_dir + " && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                              "-s 5 -x 5 -k 1M -o '" + output + "' '" + url + "' 2>/dev/null || "
                              "curl -L -o '" + output + "' '" + url + "' 2>/dev/null || true");
        };

        std::string common_url = grep_tiger_vnc_deb_file(deb_repo, "tigervnc-common", "deb10");
        std::string server_url = grep_tiger_vnc_deb_file(deb_repo, "tigervnc-standalone-server", "deb10");
        std::string jpeg_url = grep_tiger_vnc_deb_file(
            "https://mirrors.bfsu.edu.cn/debian/pool/main/libj/libjpeg-turbo/", "libjpeg62-turbo_", "deb");

        download_deb(common_url, "tigervnc-common_ubuntu-focal.deb");
        download_deb(server_url, "tigervnc-standalone-server_ubuntu-focal.deb");
        download_deb(jpeg_url, "libjpeg62-turbo_ubuntu-focal.deb");

        run_shell_command("cd " + tmp_dir + " && "
                          "dpkg -i ./libjpeg62-turbo_ubuntu-focal.deb "
                          "./tigervnc-common_ubuntu-focal.deb "
                          "./tigervnc-standalone-server_ubuntu-focal.deb 2>/dev/null || true");
        run_shell_command("apt-mark hold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
        run_shell_command(CommandBuilder("rm").add_flag("-rvf").add_arg(tmp_dir).add_raw("2>/dev/null || true").build_string());
    }

    void VncManager::case_debian_distro_and_install_vnc() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        if (family != DistroFamily::Debian) return;

        bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
        if (is_ubuntu) {
            auto os_check = Executor::shell("grep -Eq 'Focal Fossa|focal|Eoan Ermine' /etc/os-release && echo 'yes'");
            bool is_focal_like = os_check.ok() && os_check.stdout_data.find("yes") != std::string::npos;
            if (is_focal_like && vnc_config_.server_bin == "tigervnc") {
                auto ver_check = Executor::shell("apt list --installed 2>&1 | grep 'tigervnc-standalone-server' | "
                    "awk '{print $2}' | grep '1.9.'");
                if (!ver_check.ok() || ver_check.stdout_data.empty()) {
                    ubuntu_install_tiger_vnc_server();
                }
            } else if (is_focal_like) {
                run_shell_command("apt-mark unhold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
                debian_install_vnc_server();
            } else {
                debian_install_vnc_server();
            }
        } else {
            debian_install_vnc_server();
        }
        if (!vnc_config_.server.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i").add_flag("-E")
                              .add_arg("s@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server + "@")
                              .add_arg("/usr/local/bin/startvnc").add_raw("2>/dev/null || true").build_string());
        }
    }

    void VncManager::which_vnc_server_do_you_prefer() {
        // 根据 REMOTE_DESKTOP_SESSION 推荐
        bool recommend_tiger = false;
        if (Executor::has("startplasma-x11") || Executor::has("startlxqt") ||
            Executor::has("gnome-shell") || Executor::has("cinnamon-session") ||
            Executor::has("startdde") || Executor::has("ukui-session") || Executor::has("budgie-desktop")) {
            recommend_tiger = true;
        }
        choose_vnc_server();
        modify_to_xfwm4_breeze_theme();
        if (!vnc_config_.server.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i").add_flag("-E")
                              .add_arg("s@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server + "@")
                              .add_arg("/usr/local/bin/startvnc").add_raw("2>/dev/null || true").build_string());
        }
    }

    void VncManager::modify_to_xfwm4_breeze_theme() {
        if (fs::exists("/usr/share/themes/Breeze/xfwm4/themerc")) {
            run_shell_command(
                "dbus-launch xfconf-query -c xfwm4 -t string -np /general/theme -s Breeze 2>/dev/null || true");
        }
    }

    void VncManager::create_the_which_script() {
        std::string which_file = "/usr/local/bin/which";
        if (!fs::exists(which_file)) {
            Logger::info(_("gui.vnc.creating_which"));
            write_file_content(fs::path(which_file), "#!/bin/sh\ncommand -v \"$@\"\n");
            run_shell_command(CommandBuilder("chmod").add_arg("a+rx").add_arg(which_file).build_string());
        }
    }

    void VncManager::check_the_which_command() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Debian) {
            create_the_which_script();
        } else {
            if (!fs::exists("/usr/bin/which")) create_the_which_script();
        }
    }

    bool VncManager::is_arm_container() const {
        auto arch_res = Executor::shell("dpkg --print-architecture 2>/dev/null || uname -m");
        std::string arch = arch_res.ok() ? arch_res.stdout_data : "";
        while (!arch.empty() && (arch.back() == '\n' || arch.back() == '\r')) arch.pop_back();

        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();

        if (family == DistroFamily::RedHat) {
            // arm* on RedHat → skip noVNC
            return (arch.rfind("arm", 0) == 0);
        }
        // On other distros: armhf or armel → skip noVNC
        return (arch == "armhf" || arch == "armel");
    }

    void VncManager::auto_select_keyboard_layout() {
        run_shell_command("printf '%s\\n' 'debconf debconf/frontend select Noninteractive' | "
            "debconf-set-selections 2>/dev/null || true");
        run_shell_command(
            "printf '%s\\n' \"keyboard-configuration keyboard-configuration/layout select 'English (US)'\" | "
            "debconf-set-selections 2>/dev/null || true");
        run_shell_command("echo keyboard-configuration keyboard-configuration/layoutcode select 'us' | "
            "debconf-set-selections 2>/dev/null || true");
    }

    void VncManager::fix_mlocate() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family != DistroFamily::Debian) return;
        if (cfg_.sub_distro != "ubuntu") return;

        auto ver_check = Executor::shell(
            "grep -qE 'Focal Fossa|focal|bionic|Bionic Beaver|xenial|Xenial Xerus|impish|Impish Indri' "
            "/etc/os-release && echo 'old' || echo 'new'");
        if (!ver_check.ok() || ver_check.stdout_data.find("old") == std::string::npos) return;

        Logger::info(_("gui.vnc.fixing_mlocate"));
        std::string postinst_dst = "/var/lib/dpkg/info/mlocate.postinst";
        if (fs::exists(postinst_dst)) {
            // 内联修复: 在 set -e 后插入 exit 0，防止旧版 Ubuntu 下 dpkg 中断
            run_shell_command(CommandBuilder("sed").add_flag("-i")
                .add_arg("s@set -e@set -e\\nexit 0@").add_arg(postinst_dst)
                .add_raw("2>/dev/null || true").build_string());
            Logger::ok(_("gui.vnc.mlocate_fixed"));
        } else {
            Logger::debug("mlocate postinst not found, skipping fix");
        }
    }

    bool VncManager::choose_vnc_server() {
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"" + std::string(_("gui.vnc_server_title")) + "\""
                               " --menu \"" + std::string(_("gui.vnc_server_prompt")) + "\" 0 0 0 "
                               "\"tiger\" \"" + std::string(_("gui.vnc_server_tiger")) + "\" "
                               "\"tight\" \"" + std::string(_("gui.vnc_server_tight")) + "\" ";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice.empty()) choice = "tiger";

        if (choice == "tight") {
            vnc_config_.server = "tight";
            vnc_config_.server_bin = "tightvnc";
            vnc_config_.dep_server = "tightvncserver";
            vnc_config_.dep_viewer = "tigervnc-viewer";
            Logger::info(_("gui.vnc.selected_tight"));
        } else {
            vnc_config_.server = "tiger";
            vnc_config_.server_bin = "tigervnc";
            vnc_config_.dep_server = "tigervnc-standalone-server";
            vnc_config_.dep_viewer = "tigervnc-viewer";
            Logger::info(_("gui.vnc.selected_tiger"));
        }
        return true;
    }

    // ================================================================
    // 密码与默认配置
    // ================================================================

    bool VncManager::configure_vnc_password(std::string_view password) {
        Logger::step(_("gui.vnc.password"));

        if (!ensure_vnc_home_dir()) {
            Logger::error(_("gui.vnc.create_dir_failed"));
            return false;
        }

        std::string passwd_to_use;
        if (!password.empty()) {
            passwd_to_use = std::string(password);
        } else {
            std::string pass_cmd = cfg_.tui_bin +
                                   " --title \"" + std::string(_("gui.vnc_passwd_title")) +
                                   "\" --passwordbox \"" + std::string(_("gui.vnc_passwd_prompt")) + "\" 10 40";
            passwd_to_use = Executor::tui_select(pass_cmd);
            if (passwd_to_use.empty()) {
                Logger::warn(_("gui.vnc.no_password"));
                return true;
            }
        }

        // 密码长度检查
        if (passwd_to_use.length() < 6 || passwd_to_use.length() > 8) {
            std::string msg = (passwd_to_use.length() < 6)
                                  ? _("gui.vnc.password_too_short")
                                  : _("gui.vnc.password_too_long");
            Logger::warn(msg);
            run_shell_command(cfg_.tui_bin +
                              " --title \"" + std::string(_("gui.vnc.password_length_error_title")) + "\""
                              " --msgbox \"" + msg + "\" 10 50");
            if (password.empty()) {
                return configure_vnc_password("");
            } else {
                Logger::info(_("gui.vnc.default_password_info"));
                passwd_to_use = "tmoe123";
            }
        }

        // 写入密码文件
        std::string cmd;
        if (Executor::has("vncpasswd")) {
            cmd = "echo '" + passwd_to_use + "' | vncpasswd -f > " +
                  vnc_config_.passwd_file.string() + " 2>/dev/null";
        } else {
            cmd = CommandBuilder("x11vnc").add_arg("-storepasswd").add_arg(passwd_to_use)
                      .add_arg(vnc_config_.passwd_file.string()).add_raw("2>/dev/null").build_string();
        }

        if (run_shell_command(cmd, false)) {
            vnc_config_.password = passwd_to_use;
            if (cfg_.is_root) {
                run_shell_command(CommandBuilder("chmod").add_arg("600").add_arg(vnc_config_.passwd_file.string()).add_raw("2>/dev/null").build_string());
            }
            Logger::ok(_f("gui.vnc.password_set", vnc_config_.passwd_file.string()));
            // 复制到 x11passwd
            fs::path x11passwd = vnc_config_.vnc_home_dir / "x11passwd";
            if (fs::exists(vnc_config_.passwd_file)) {
                run_shell_command(CommandBuilder("cp").add_arg(vnc_config_.passwd_file.string())
                    .add_arg(x11passwd.string()).add_raw("2>/dev/null || true").build_string());
                run_shell_command(CommandBuilder("chmod").add_arg("600").add_arg(x11passwd.string()).add_raw("2>/dev/null || true").build_string());
            }
            return true;
        }

        Logger::error(_("gui.vnc.password_failed"));
        return false;
    }

    bool VncManager::configure_vnc_defaults() {
        Logger::step(_("gui.vnc.configuring_defaults"));

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

        return write_file_content(vnc_config_.tigervnc_config, config.str());
    }

    // ================================================================
    // 配置内容生成 (桌面会话脚本)
    // ================================================================

    namespace {
        struct SessionCmds {
            std::string cmd1;
            std::string cmd2;
        };

        SessionCmds lookup_session_commands(std::string_view desktop) {
            std::string d(desktop);
            std::transform(d.begin(), d.end(), d.begin(), ::tolower);
            if (d == "xfce" || d == "xfce-lite") return {"xfce4-session", "startxfce4"};
            if (d == "lxqt") return {"lxqt-session", "startlxqt"};
            if (d == "lxde") return {"lxsession", "startlxde"};
            if (d == "mate") return {"mate-session", "mate-panel"};
            if (d == "kde") return {"startplasma-x11", "startkde"};
            if (d == "cinnamon") return {"cinnamon-session", "cinnamon"};
            if (d == "gnome") return {"gnome-session", "gnome-shell-x11"};
            if (d == "budgie") return {"budgie-desktop", "budgie-panel"};
            if (d == "dde") return {"startdde", "dde-launcher"};
            if (d == "deepin") return {"deepin-session", "deepin-launcher"};
            if (d == "ukui") return {"ukui-session", "ukui-panel"};
            if (d == "cutefish") return {"cutefish-session", "cutefish-launcher"};
            return {"xfce4-session", "startxfce4"};
        }
    }

    std::string VncManager::generate_xsession_content(std::string_view desktop) const {
        SessionCmds cmds = lookup_session_commands(desktop);

        std::ostringstream script;
        script << "#!/bin/bash\n"
                << "# tmoe-linux Xsession — 自动生成\n"
                << "unset WAYLAND_DISPLAY\n"
                << "unset XDG_RUNTIME_DIR\n"
                << "export LIBGL_ALWAYS_SOFTWARE=1\n"
                << "export GALLIUM_DRIVER=llvmpipe\n\n"
                << "SESSION_01=\"" << cmds.cmd1 << "\"\n"
                << "SESSION_02=\"" << cmds.cmd2 << "\"\n\n"
                << "set_session_env() {\n"
                << "    if command -v \"$SESSION_01\" >/dev/null 2>&1; then\n"
                << "        REMOTE_DESKTOP_SESSION=\"$SESSION_01\"\n"
                << "    elif command -v \"$SESSION_02\" >/dev/null 2>&1; then\n"
                << "        REMOTE_DESKTOP_SESSION=\"$SESSION_02\"\n"
                << "    else\n"
                << "        echo \"Error: desktop session command not found\"\n"
                << "        exit 1\n"
                << "    fi\n"
                << "    [[ ! -s /etc/environment ]] || source /etc/environment\n"
                << "}\n\n"
                << "open_terminal() {\n"
                << "    for term in xfce4-terminal x-terminal-emulator konsole gnome-terminal lxterminal mate-terminal; do\n"
                << "        if command -v \"$term\" >/dev/null 2>&1; then\n"
                << "            \"$term\" & break\n"
                << "        fi\n"
                << "    done\n"
                << "}\n\n"
                << "start_session() {\n"
                << "    set_session_env\n"
                << "    open_terminal\n"
                << "    echo \"Starting desktop session: $REMOTE_DESKTOP_SESSION\"\n";
        bool is_proot = cfg_.is_termux || cfg_.linux_distro == "Android";
        script << "    exec dbus-launch"
                << (is_proot ? "" : " --exit-with-session")
                << " $REMOTE_DESKTOP_SESSION \"$@\"\n"
                << "}\n\n"
                << "start_session\n";
        return script.str();
    }

    std::string VncManager::generate_xstartup_content() const {
        std::ostringstream script;
        script << "#!/bin/bash\n"
                << "# tmoe-linux VNC xstartup — link to Xsession\n\n"
                << "unset SESSION_MANAGER\n"
                << "unset DBUS_SESSION_BUS_ADDRESS\n"
                << "export XDG_SESSION_TYPE=x11\n"
                << "export XDG_CURRENT_DESKTOP=XFCE\n"
                << "export XDG_RUNTIME_DIR=/tmp/runtime-${USER:-root}\n"
                << "export DESKTOP_SESSION=tmoe_linux\n"
                << "[ -d \"$XDG_RUNTIME_DIR\" ] || mkdir -p \"$XDG_RUNTIME_DIR\" 2>/dev/null\n"
                << "[ -w \"$XDG_RUNTIME_DIR\" ] || chmod 700 \"$XDG_RUNTIME_DIR\" 2>/dev/null\n\n"
                << "if command -v fcitx >/dev/null 2>&1; then\n"
                << "    fcitx-autostart >/dev/null 2>&1 &\n"
                << "fi\n"
                << "if command -v ibus-daemon >/dev/null 2>&1; then\n"
                << "    ibus-daemon -drx >/dev/null 2>&1 &\n"
                << "fi\n\n"
                << "if [ -x " << vnc_config_.xsession_file.string() << " ]; then\n"
                << "    exec " << vnc_config_.xsession_file.string() << "\n"
                << "else\n"
                << "    xterm -geometry 80x24+10+10 -ls -title \"tmoe VNC Desktop\" &\n"
                << "    exec twm\n"
                << "fi\n";
        return script.str();
    }

    bool VncManager::configure_xstartup(std::string_view desktop) {
        Logger::step(_f("gui.vnc.configuring_xsession", std::string(desktop)));

        // 1. 创建 Xsession
        std::string xsession_content = generate_xsession_content(desktop);
        if (!write_file_content(vnc_config_.xsession_file, xsession_content)) {
            Logger::error(_f("gui.vnc.xsession_write_failed", vnc_config_.xsession_file.string()));
            return false;
        }
        run_shell_command(CommandBuilder("chmod").add_arg("777").add_arg(vnc_config_.xsession_file.string()).build_string());

        // 2. 确保 DBus 环境
        run_shell_command("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
        run_shell_command("ln -svf /run /var/ 2>/dev/null || true");
        if (!fs::exists("/etc/machine-id")) {
            run_shell_command("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
                "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
        }
        if (!fs::exists("/etc/machine-id") || fs::file_size("/etc/machine-id") == 0) {
            write_file_content("/etc/machine-id", "0ecb780817003d3342d16adb5ff1dfa9\n");
        }
        if (!fs::exists("/var/lib/dbus/machine-id")) {
            run_shell_command("ln -svf /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
        }

        // 3. 创建 ~/.vnc/xstartup
        run_shell_command("mkdir -p ~/.vnc /etc/X11/xinit /etc/tigervnc 2>/dev/null");
        std::string xstartup_content = generate_xstartup_content();
        if (!write_file_content(vnc_config_.xstartup_file, xstartup_content)) {
            Logger::error(_f("gui.vnc.xstartup_write_failed", vnc_config_.xstartup_file.string()));
            return false;
        }
        run_shell_command(CommandBuilder("chmod").add_arg("+x").add_arg(vnc_config_.xstartup_file.string()).build_string());
        run_shell_command(CommandBuilder("ln").add_flag("-sf").add_arg(vnc_config_.xsession_file.string())
                          .add_arg(vnc_config_.xstartup_file.string()).add_raw("2>/dev/null || true").build_string());

        // 4. 默认配置
        configure_vnc_defaults();

        Logger::ok(_("gui.vnc.xsession_configured"));
        return true;
    }

    // ================================================================
    // VNC 命令构建 (使用 CommandBuilder)
    // ================================================================

    std::string VncManager::build_vnc_start_command(int display, int width, int height) const {
        if (display <= 0) display = vnc_config_.display;
        if (width <= 0) width = vnc_config_.resolution_w;
        if (height <= 0) height = vnc_config_.resolution_h;

        CommandBuilder cmd(vnc_config_.server == "tight" ? "tightvncserver" : "vncserver");
        cmd.add_arg(":" + std::to_string(display))
                .add_arg("-geometry").add_arg(std::to_string(width) + "x" + std::to_string(height))
                .add_arg("-depth")
                .add_arg(std::to_string(vnc_config_.pixel_depth));

        if (vnc_config_.server == "tight") {
            if (!vnc_config_.password.empty())
                cmd.add_arg("-passwd").add_arg(vnc_config_.passwd_file.string());
            if (vnc_config_.always_shared)
                cmd.add_arg("-alwaysshared");
        } else {
            cmd.add_arg("-localhost").add_arg("no");
            cmd.add_arg("-SecurityTypes")
                    .add_arg("VncAuth");
            if (!vnc_config_.password.empty())
                cmd.add_arg("-rfbauth").add_arg(vnc_config_.passwd_file.string());
            if (vnc_config_.zlib_level >= 0)
                cmd.add_arg("-ZlibLevel").add_arg(std::to_string(vnc_config_.zlib_level));
            if (vnc_config_.always_shared)
                cmd.add_arg("-AlwaysShared");
        }

        return cmd.build_string();
    }

    std::string VncManager::build_xvfb_command(int display, int width, int height) const {
        if (display <= 0) display = 233;
        if (width <= 0) width = vnc_config_.resolution_w;
        if (height <= 0) height = vnc_config_.resolution_h;

        CommandBuilder cmd("Xvfb");
        cmd.add_arg(":" + std::to_string(display));
        cmd.add_arg("-screen").add_arg("0").add_arg(std::to_string(width) + "x" + std::to_string(height) + "x24");
        cmd.add_arg("-ac");
        cmd.add_arg("+extension").add_arg("RANDR");
        cmd.add_arg("&"); // background
        return cmd.build_string();
    }

    std::string VncManager::build_x11vnc_command(int display) const {
        if (display <= 0) display = 233;

        CommandBuilder cmd("x11vnc");
        cmd.add_arg("-display").add_arg(":" + std::to_string(display));
        cmd.add_arg("-ncache_cr");
        cmd.add_arg("-xkb");
        cmd.add_arg("-noxrecord");
        cmd.add_arg("-noxdamage");
        cmd.add_arg("-forever");
        cmd.add_arg("-bg");
        cmd.add_arg("-noshm");
        cmd.add_arg("-cursor").add_arg("arrow");
        cmd.add_arg("-rfbport").add_arg(std::to_string(display));

        fs::path x11_passwd = vnc_config_.vnc_home_dir / "x11passwd";
        if (fs::exists(x11_passwd)) {
            cmd.add_arg("-rfbauth").add_arg(x11_passwd.string());
        } else if (!vnc_config_.password.empty()) {
            cmd.add_arg("-rfbauth").add_arg(vnc_config_.passwd_file.string());
        }

        return cmd.build_string();
    }

    // ================================================================
    // VNC 启动/停止
    // ================================================================

    bool VncManager::start_vnc(int display, int width, int height) {
        Logger::step(_("gui.vnc.start"));

        if (display > 0) {
            vnc_config_.display = display;
            vnc_config_.update_port();
        }
        if (width > 0) vnc_config_.resolution_w = width;
        if (height > 0) vnc_config_.resolution_h = height;

        if (!check_xvnc_command()) {
            Logger::error(_("gui.vnc.server_not_available"));
            return false;
        }

        if (!fs::exists(vnc_config_.xstartup_file)) {
            Logger::warn(_("gui.vnc.xstartup_missing"));
            configure_xstartup("xfce");
        }

        if (is_vnc_running()) {
            Logger::warn(_f("gui.vnc.already_running", std::to_string(vnc_config_.display)));
            return true;
        }

        // 清理残留锁
        std::string lock_path = "/tmp/.X" + std::to_string(vnc_config_.display) + "-lock";
        std::string socket_path = "/tmp/.X11-unix/X" + std::to_string(vnc_config_.display);
        run_shell_command(CommandBuilder("rm").add_flag("-f").add_arg(lock_path).add_arg(socket_path).add_raw("2>/dev/null").build_string());

        // WSL 处理
        std::string env_prefix;
        if (cfg_.is_wsl) {
            detect_wsl_environment();
            env_prefix = "unset WAYLAND_DISPLAY; ";
            if (!vnc_config_.windows_ip.empty() && vnc_config_.windows_ip != "127.0.0.1") {
                env_prefix += "export PULSE_SERVER=" + vnc_config_.windows_ip + "; ";
                Logger::info(_f("gui.vnc.wsl2_pulseaudio", vnc_config_.windows_ip));
            }
        }

        launch_dbus_daemon();

        std::string cmd = build_vnc_start_command(display, width, height);
        ExecResult result = Executor::passthrough(env_prefix + cmd + " > /tmp/tmoe_vnc_startup.log 2>&1");

        if (result.ok()) {
            Logger::ok(_f("gui.vnc.started",
                          std::to_string(vnc_config_.rfb_port) + " (display :" +
                          std::to_string(vnc_config_.display) + ")"));
            Logger::info(_f("gui.vnc.resolution_info",
                            std::to_string(vnc_config_.resolution_w) + "x" + std::to_string(vnc_config_.resolution_h)));
            Logger::info(_f("gui.vnc.connection_address", get_vnc_connection_uri()));

            std::string ips = get_local_ip_addresses();
            if (!ips.empty()) {
                Logger::info(_f("gui.vnc.lan_address", ips));
            }

            write_vnc_pid_file(vnc_config_.display);
            return true;
        }

        Logger::error(_("gui.vnc.start_failed"));
        return false;
    }

    bool VncManager::stop_vnc(int display) {
        Logger::step(_("gui.vnc.stop"));

        if (display <= 0) display = vnc_config_.display;

        // 1. vncserver -kill
        run_shell_command(CommandBuilder("vncserver").add_flag("-kill").add_arg(":" + std::to_string(display)).add_raw("2>/dev/null").build_string());

        // 2. 基于 PID 文件
        remove_vnc_pid_file(display);

        // 3. pkill 精确匹配
        run_shell_command("pkill -f 'Xvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        run_shell_command("pkill -f 'Xtigervnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
        run_shell_command("pkill -f 'Xtightvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");

        // 4. 清理锁
        run_shell_command(CommandBuilder("rm").add_flag("-f").add_arg("/tmp/.X" + std::to_string(display) + "-lock").add_raw("2>/dev/null").build_string());
        run_shell_command(CommandBuilder("rm").add_flag("-f").add_arg("/tmp/.X11-unix/X" + std::to_string(display)).add_raw("2>/dev/null").build_string());

        // 5. 停止 websockify
        run_shell_command("pkill -f 'websockify.*:" + std::to_string(vnc_config_.rfb_port) + "' 2>/dev/null || true");

        // 6. 彻底清理
        kill_all_vnc();

        Logger::ok(_f("gui.vnc.stopped", std::to_string(display)));
        return true;
    }

    bool VncManager::start_x11vnc(int display) {
        Logger::step(_("gui.x11vnc.starting"));

        int x11_display = (display > 0) ? display : 233;

        // 1. Xvfb
        std::string xvfb_cmd = build_xvfb_command(x11_display, 0, 0);
        Logger::debug("Executing: " + xvfb_cmd);
        run_shell_command(xvfb_cmd);
        run_shell_command("sleep 2");

        // 2. x11vnc
        std::string x11vnc_cmd = build_x11vnc_command(x11_display);
        Logger::debug("Executing: " + x11vnc_cmd);
        ExecResult result = Executor::passthrough(x11vnc_cmd);

        if (result.ok()) {
            Logger::ok(_f("gui.x11vnc.started", std::to_string(vnc_config_.rfb_port)));
            return true;
        }

        Logger::error(_("gui.x11vnc.start_failed"));
        return false;
    }

    bool VncManager::stop_x11vnc() {
        Logger::step(_("gui.x11vnc.stopping"));
        run_shell_command("pkill -f x11vnc 2>/dev/null || true");
        run_shell_command("pkill Xvfb 2>/dev/null || true");
        run_shell_command("rm -f /tmp/.X233-lock /tmp/.X11-unix/X233 2>/dev/null || true");
        Logger::ok(_("gui.x11vnc.stopped"));
        return true;
    }

    bool VncManager::kill_all_vnc() {
        Logger::step(_("gui.vnc.killing_all"));
        run_shell_command("pkill Xtightvnc 2>/dev/null || true");
        run_shell_command("pkill Xtigervnc 2>/dev/null || true");
        run_shell_command("pkill Xvnc 2>/dev/null || true");
        run_shell_command("pkill x11vnc 2>/dev/null || true");
        run_shell_command("pkill Xvfb 2>/dev/null || true");
        run_shell_command("rm -f /tmp/.X*-lock /tmp/.X11-unix/X* 2>/dev/null || true");
        Logger::ok(_("gui.vnc.killed_all"));
        return true;
    }

    // ================================================================
    // 运行时状态查询
    // ================================================================

    bool VncManager::is_vnc_running(int display) const {
        if (display <= 0) display = vnc_config_.display;
        auto result = Executor::shell("pgrep -f 'X(vnc|tigervnc|tightvnc).*:" +
                                      std::to_string(display) + "' 2>/dev/null");
        if (result.ok() && !result.stdout_data.empty()) return true;
        if (fs::exists("/tmp/.X" + std::to_string(display) + "-lock")) return true;
        return false;
    }

    int VncManager::detect_available_display() const {
        for (int d = 1; d <= 10; ++d) {
            if (!fs::exists("/tmp/.X" + std::to_string(d) + "-lock")) return d;
        }
        return 1;
    }

    std::string VncManager::get_vnc_connection_uri() const {
        return "vnc://127.0.0.1:" + std::to_string(vnc_config_.rfb_port);
    }

    std::string VncManager::get_local_ip_addresses() const {
        std::ostringstream ips;
        auto v4 = Executor::shell(
            "ip -4 addr show scope global | grep inet | awk '{print $2}' | cut -d/ -f1 2>/dev/null");
        if (v4.ok() && !v4.stdout_data.empty()) {
            std::string data = v4.stdout_data;
            std::istringstream iss(data);
            std::string line;
            while (std::getline(iss, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                    line.
                            pop_back();
                while (!line.empty() && (line.front() == ' ')) line.erase(0, 1);
                if (!line.empty()) ips << "vnc://" << line << ":" << vnc_config_.rfb_port << "  ";
            }
        }
        if (ips.str().empty()) {
            auto v4_host = Executor::shell(
                "hostname -I 2>/dev/null | awk '{for(i=1;i<=NF;i++){if($i ~ /^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$/ && $i != \"127.0.0.1\"){print $i;exit}}}'");
            if (v4_host.ok() && !v4_host.stdout_data.empty()) {
                std::string ip = v4_host.stdout_data;
                while (!ip.empty() && (ip.back() == '\r' || ip.back() == '\n' || ip.back() == ' ')) ip.pop_back();
                if (!ip.empty()) ips << "vnc://" << ip << ":" << vnc_config_.rfb_port;
            }
        }
        return ips.str();
    }

    // ================================================================
    // PID 管理
    // ================================================================

    void VncManager::write_vnc_pid_file(int display) const {
        std::ostringstream cmd;
        cmd << "pgrep -f 'X(vnc|tigervnc|tightvnc).*:" << display << "' | head -1 > "
                << vnc_config_.vnc_pid_file.string() << " 2>/dev/null";
        run_shell_command(cmd.str());
        run_shell_command("pgrep -f 'X(tigervnc|tightvnc).*:" + std::to_string(display) + "' | head -1 > "
                          + vnc_config_.x_pid_file.string() + " 2>/dev/null || true");
    }

    void VncManager::remove_vnc_pid_file(int display) const {
        if (fs::exists(vnc_config_.vnc_pid_file)) {
            auto pid_data = SystemHelper::read_file(vnc_config_.vnc_pid_file);
            if (!pid_data.empty()) {
                std::string mypid = pid_data;
                while (!mypid.empty() && (mypid.back() == '\n' || mypid.back() == '\r')) mypid.pop_back();
                if (!mypid.empty()) run_shell_command(CommandBuilder("kill").add_arg(mypid).add_raw("2>/dev/null || true").build_string());
            }
            fs::remove(vnc_config_.vnc_pid_file);
        }
        if (fs::exists(vnc_config_.x_pid_file)) {
            auto pid_data = SystemHelper::read_file(vnc_config_.x_pid_file);
            if (!pid_data.empty()) {
                std::string mypid = pid_data;
                while (!mypid.empty() && (mypid.back() == '\n' || mypid.back() == '\r')) mypid.pop_back();
                if (!mypid.empty()) run_shell_command(CommandBuilder("kill").add_arg(mypid).add_raw("2>/dev/null || true").build_string());
            }
            fs::remove(vnc_config_.x_pid_file);
        }
    }

    // ================================================================
    // 环境检测
    // ================================================================

    bool VncManager::detect_android_resolution(int &width, int &height) const {
        if (!cfg_.is_termux) return false;

        auto result = Executor::shell("wm size 2>/dev/null");
        if (!result.ok() || result.stdout_data.empty()) return false;

        std::string output = result.stdout_data;
        auto pos = output.find(':');
        if (pos == std::string::npos) return false;

        std::string size_str = output.substr(pos + 1);
        size_str.erase(0, size_str.find_first_not_of(" \t\n\r"));
        auto x_pos = size_str.find('x');
        if (x_pos == std::string::npos) return false;

        try {
            width = std::stoi(size_str.substr(0, x_pos));
            height = std::stoi(size_str.substr(x_pos + 1));
            if (width < height) {
                int tmp = width;
                width = height / 2;
                height = tmp / 2;
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void VncManager::detect_wsl_environment() {
        auto wsl_field = Executor::shell("uname -r | cut -d '-' -f 2 2>/dev/null");
        std::string field2 = wsl_field.ok() ? wsl_field.stdout_data : "";
        while (!field2.empty() && (field2.back() == '\n' || field2.back() == '\r')) field2.pop_back();
        if (field2 == "microsoft") {
            auto route = Executor::shell(
                "ip route list table 0 | head -n 1 | awk -F 'default via ' '{print $2}' | awk '{print $1}' 2>/dev/null");
            if (route.ok() && !route.stdout_data.empty()) {
                std::string ip = route.stdout_data;
                while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r')) ip.pop_back();
                vnc_config_.windows_ip = ip;
            }
            Logger::info(_f("gui.vnc.wsl2_gateway", vnc_config_.windows_ip));
        } else {
            vnc_config_.windows_ip = "127.0.0.1";
            Logger::info(_("gui.vnc.wsl1_detected"));
        }
    }

    // ================================================================
    // 权限 / D-Bus / 脚本部署
    // ================================================================

    bool VncManager::fix_vnc_permissions() {
        Logger::step(_("gui.vnc.fixing_permissions"));
        run_shell_command("chown -R $(id -un):$(id -gn) " +
                          vnc_config_.vnc_home_dir.string() + " 2>/dev/null || true");
        run_shell_command(CommandBuilder("chmod").add_flag("-R").add_arg("700").add_arg(vnc_config_.vnc_home_dir.string()).add_raw("2>/dev/null || true").build_string());
        if (fs::exists(vnc_config_.passwd_file))
            run_shell_command(CommandBuilder("chmod").add_arg("600").add_arg(vnc_config_.passwd_file.string()).add_raw("2>/dev/null || true").build_string());
        Logger::ok(_("gui.vnc.permissions_fixed"));
        return true;
    }

    bool VncManager::deploy_startup_scripts() {
        Logger::step(_("gui.vnc.deploying_scripts"));

        const char *tmoe_bin = "/usr/local/bin/tmoe";
        if (!fs::exists(tmoe_bin)) {
            Logger::warn(_("gui.vnc.tmoe_not_found"));
        }

        auto write_script = [&](const std::string &name, const std::string &content) {
            write_file_content(fs::path("/usr/local/bin/" + name), content);
            run_shell_command(CommandBuilder("chmod").add_arg("+x").add_arg("/usr/local/bin/" + name).build_string());
        };

        // startvnc
        write_script("startvnc",
                     "#!/bin/bash\n# tmoe-linux startvnc — thin wrapper\n"
                     "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                     "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-vnc \"$@\"\n"
                     "echo 'Please install tmoe: ln -s $(which tmoe) /usr/local/bin/tmoe'\n");

        // stopvnc
        write_script("stopvnc",
                     "#!/bin/bash\n# tmoe-linux stopvnc — thin wrapper\n"
                     "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                     "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --stop-vnc \"$@\"\n"
                     "pkill Xvnc; pkill Xtigervnc; pkill Xtightvnc; pkill x11vnc; pkill websockify 2>/dev/null\n"
                     "echo 'VNC stopped (fallback)'\n");

        // startxsdl
        write_script("startxsdl",
                     "#!/bin/bash\n# tmoe-linux startxsdl — thin wrapper\n"
                     "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                     "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-xsdl \"$@\"\n"
                     "export DISPLAY=\"${1:-127.0.0.1:0}\"\n"
                     "export PULSE_SERVER=\"tcp:${DISPLAY%:*}:4713\"\n"
                     "echo \"XSDL DISPLAY=$DISPLAY PULSE_SERVER=$PULSE_SERVER\"\n");

        // startx11vnc
        write_script("startx11vnc",
                     "#!/bin/bash\n# tmoe-linux startx11vnc — thin wrapper\n"
                     "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                     "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-x11vnc \"$@\"\n"
                     "Xvfb :233 -screen 0 1440x720x24 -ac +extension RANDR &\n"
                     "sleep 2; x11vnc -display :233 -rfbport 5902 -forever -shared -nopw -bg\n"
                     "echo 'x11vnc started on port 5902 (fallback)'\n");

        // novnc
        write_script("novnc",
                     "#!/bin/bash\n# tmoe-linux novnc — thin wrapper\n"
                     "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                     "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-novnc \"$@\"\n"
                     "NOVNC_DIR=/usr/share/novnc; [ -d \"$NOVNC_DIR\" ] || NOVNC_DIR=/opt/novnc\n"
                     "websockify --web=\"$NOVNC_DIR\" \"${1:-36080}\" localhost:${2:-5902} &\n"
                     "echo \"noVNC: http://localhost:${1:-36080}/vnc.html (fallback)\"\n");

        // symlinks
        run_shell_command("ln -sf /usr/local/bin/startvnc /usr/local/bin/tightvnc 2>/dev/null || true");
        run_shell_command("ln -sf /usr/local/bin/startvnc /usr/local/bin/tigervnc 2>/dev/null || true");

        Logger::ok(_("gui.vnc.scripts_deployed"));
        return true;
    }

    bool VncManager::launch_dbus_daemon() {
        if (fs::exists("/var/run/dbus/pid") || fs::exists("/run/dbus/pid")) {
            return true;
        }

        Logger::debug("Starting D-Bus daemon...");
        if (run_shell_command("service dbus start 2>/dev/null")) return true;
        if (run_shell_command("systemctl start dbus 2>/dev/null")) return true;

        run_shell_command("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
        if (run_shell_command("dbus-daemon --system --fork 2>/dev/null")) return true;

        bool is_proot = cfg_.is_termux || cfg_.linux_distro == "Android";
        Logger::debug("Using dbus-launch for session dbus" + std::string(is_proot ? " (proot mode)" : ""));
        return run_shell_command(is_proot
                                     ? "dbus-launch 2>/dev/null &"
                                     : "dbus-launch --exit-with-session 2>/dev/null &");
    }

    bool VncManager::fix_vnc_dbus() {
        Logger::step(_("gui.vnc.fixing_dbus"));
        run_shell_command("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");

        if (!fs::exists("/etc/machine-id")) {
            run_shell_command("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
                "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
        }
        if (!fs::exists("/etc/machine-id") || fs::file_size("/etc/machine-id") == 0) {
            write_file_content("/etc/machine-id", "0ecb780817003d3342d16adb5ff1dfa9\n");
        }
        if (!fs::exists("/var/lib/dbus/machine-id")) {
            run_shell_command("ln -svf /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
        }

        Logger::ok(_("gui.vnc.dbus_fixed"));
        return true;
    }

    bool VncManager::stop_dbus_daemon() {
        Logger::debug("Stopping D-Bus daemon...");

        if (fs::exists("/run/dbus/pid")) {
            auto pid_data = SystemHelper::read_file("/run/dbus/pid");
            if (!pid_data.empty()) run_shell_command(CommandBuilder("kill").add_arg(pid_data).add_raw("2>/dev/null || true").build_string());
            fs::remove("/run/dbus/pid");
        }
        if (fs::exists("/var/run/dbus/pid")) {
            auto pid_data = SystemHelper::read_file("/var/run/dbus/pid");
            if (!pid_data.empty()) run_shell_command(CommandBuilder("kill").add_arg(pid_data).add_raw("2>/dev/null || true").build_string());
            fs::remove("/var/run/dbus/pid");
        }

        run_shell_command("service dbus stop 2>/dev/null || systemctl stop dbus 2>/dev/null || "
            "pkill dbus-daemon 2>/dev/null || true");
        run_shell_command("pkill dbus-launch 2>/dev/null || true");

        Logger::ok(_("gui.vnc.dbus_stopped"));
        return true;
    }

    void VncManager::show_dbus_status() const {
        if (fs::exists("/run/dbus/pid") || fs::exists("/var/run/dbus/pid")) {
            Logger::info(_("gui.vnc.dbus_running"));
        } else {
            auto result = Executor::shell("pgrep dbus-daemon 2>/dev/null");
            if (result.ok() && !result.stdout_data.empty()) {
                Logger::info(_f("gui.vnc.dbus_running_pid", result.stdout_data));
            } else {
                Logger::info(_("gui.vnc.dbus_not_running"));
            }
        }
    }

    // ================================================================
    // 配置脚本
    // ================================================================

    void VncManager::configure_startvnc() {
        deploy_startup_scripts();
        Logger::ok(_("gui.vnc.scripts_configured"));
    }

    void VncManager::configure_startxsdl() {
        // deploy_startup_scripts already includes startxsdl
        if (cfg_.is_wsl) {
            // 原生生成 wslg thin wrapper (替代旧版 old-version 拷贝)
            const char *tmoe_bin = "/usr/local/bin/tmoe";
            write_file_content("/usr/local/bin/wslg",
                               "#!/bin/bash\n"
                               "# tmoe-linux wslg — native thin wrapper\n"
                               "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                               "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-wslg \"$@\"\n"
                               "# Fallback: direct Xwayland for WSLg\n"
                               "unset WAYLAND_DISPLAY\n"
                               "Xwayland :${1:-2} -noreset &\n"
                               "echo 'WSLg Xwayland started on :${1:-2}'\n");
            run_shell_command("chmod a+rx /usr/local/bin/wslg 2>/dev/null || true");
        }
        if (cfg_.sub_distro == "centos") {
            run_shell_command("sed -i -E 's@(AUTO_START_DBUS=).*@\\1false@' "
                "/usr/local/bin/startvnc /usr/local/bin/startxsdl 2>/dev/null || true");
        }
    }

    // ================================================================
    // 权限修复
    // ================================================================

    void VncManager::fix_non_root_permissions() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        if (home != "/root") {
            Logger::info(_f("gui.vnc.fixing_nonroot_perms", home));
            std::string user = Executor::shell("id -un").stdout_data;
            std::string group = Executor::shell("id -gn").stdout_data;
            while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
            while (!group.empty() && (group.back() == '\n' || group.back() == '\r')) group.pop_back();
            run_shell_command(CommandBuilder("chown").add_flag("-R").add_arg(user + ":" + group)
                              .add_arg(home + "/.vnc").add_arg(home + "/.Xauthority")
                              .add_arg(home + "/.ICEauthority").add_arg(home + "/.config")
                              .add_arg(home + "/.cache").add_arg(home + "/.dbus").add_arg(home + "/.local")
                              .add_raw("2>/dev/null || true").build_string());
        }
    }

    // ================================================================
    // VNC 配置修改
    // ================================================================

    void VncManager::check_vnc_resolution() {
        auto r = Executor::shell(
            "grep '^VNC_RESOLUTION=' /usr/local/bin/startvnc 2>/dev/null | awk -F '=' '{print $2}' | head -n 1");
        if (r.ok() && !r.stdout_data.empty()) {
            std::string res = r.stdout_data;
            while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) res.pop_back();
            auto xpos = res.find('x');
            if (xpos != std::string::npos) {
                try {
                    vnc_config_.resolution_w = std::stoi(res.substr(0, xpos));
                    vnc_config_.resolution_h = std::stoi(res.substr(xpos + 1));
                } catch (...) {
                }
            }
        }
    }

    void VncManager::modify_vnc_conf() {
        if (!fs::exists("/usr/local/bin/startvnc")) {
            Logger::warn(_("gui.vnc.startvnc_not_found"));
        }
    }

    // ================================================================
    // x11vnc 配置修改 (TUI 子菜单使用)
    // ================================================================

    void VncManager::modify_x11vnc_pulse_server() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"" + std::string(_("gui.x11vnc.modify_pulse_title")) + "\""
                          " --inputbox \"" + std::string(_("gui.x11vnc.modify_pulse_prompt")) + "\" 15 50";
        std::string addr = Executor::tui_select(cmd);
        if (!addr.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i")
                .add_arg("s@PULSE_SERVER=.*@PULSE_SERVER=" + addr + "@").add_arg("/usr/local/bin/startx11vnc")
                .add_raw("2>/dev/null || true").build_string());
        }
    }

    void VncManager::modify_x11vnc_resolution() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"" + std::string(_("gui.x11vnc.modify_resolution_title")) + "\""
                          " --inputbox \"" + std::string(_("gui.x11vnc.modify_resolution_prompt")) + "\" 10 50 \"1280x720\"";
        std::string res = Executor::tui_select(cmd);
        if (!res.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i")
                .add_arg("s@-geometry .* @-geometry " + res + " @").add_arg("/usr/local/bin/startx11vnc")
                .add_raw("2>/dev/null || true").build_string());
        }
    }

    void VncManager::modify_x11vnc_port() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"" + std::string(_("gui.x11vnc.modify_port_title")) + "\""
                          " --inputbox \"" + std::string(_("gui.x11vnc.modify_port_prompt")) + "\" 10 50 \"5902\"";
        std::string port = Executor::tui_select(cmd);
        if (!port.empty()) {
            run_shell_command(CommandBuilder("sed").add_flag("-i")
                .add_arg("s@-rfbport .* @-rfbport " + port + " @").add_arg("/usr/local/bin/startx11vnc")
                .add_raw("2>/dev/null || true").build_string());
        }
    }
} // namespace tmoe::domain
