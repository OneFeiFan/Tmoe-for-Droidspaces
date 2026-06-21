#include "vnc_manager.h"
#include "core/system_helper.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"
#include "package_manager.h"
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <map>

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
// VncManager 构造与初始化
// ═══════════════════════════════════════════════════════════════

VncManager::VncManager(const TmoeConfig &cfg) : cfg_(cfg) {
    vnc_config_.init_defaults();
}

// ═══════════════════════════════════════════════════════════════
// VNC 服务端安装与配置
// ═══════════════════════════════════════════════════════════════

bool VncManager::install_vnc_server() {
    Logger::step(_("gui.vnc.install"));

    // 安装 tigervnc standalone server + viewer
    std::vector<std::string> pkgs = {
        "tigervnc-standalone-server", "tigervnc-viewer",
        "xfonts-100dpi", "xfonts-75dpi", "xfonts-scalable",
        "x11vnc", "xvfb"
    };

    if (!SystemHelper::install_packages(pkgs, cfg_.install_command)) {
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
    // 按发行版安装对应包 (对应旧 Bash check_xvnc_command)
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
    Executor::passthrough(cfg_.install_command + " " + dep + " 2>/dev/null || " +
                          cfg_.install_command + " " + dep + " 2>/dev/null || true");

    // Arch 字体补充检测
    if (family == DistroFamily::Arch) {
        if (!fs::exists("/usr/share/fonts/noto-cjk"))
            Executor::passthrough("pacman -Syu --noconfirm --needed noto-fonts-cjk 2>/dev/null || "
                "paru -Syu --noconfirm noto-fonts-cjk 2>/dev/null || true");
        if (!fs::exists("/usr/share/fonts/noto/NotoColorEmoji.ttf"))
            Executor::passthrough("pacman -Syu --noconfirm --needed noto-fonts-emoji 2>/dev/null || "
                "paru -Syu --noconfirm noto-fonts-emoji 2>/dev/null || true");
    }

    return SystemHelper::install_packages({"tigervnc-standalone-server"}, cfg_.install_command);
}

// ═══════════════════════════════════════════════════════════════
// Phase 1: VNC 服务端安装辅助函数
// ═══════════════════════════════════════════════════════════════

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
    Logger::info("移除当前 VNC 服务端: " + current);
    Executor::passthrough("apt remove -y " + current + " 2>/dev/null || true");
}

void VncManager::debian_install_vnc_server() {
    // 对应旧 Bash debian_install_vnc_server (gui:5302-5344)
    Logger::step("Debian VNC 服务端安装 (tigervnc + tightvnc)...");

    // 安装 tigervnc-standalone-server
    if (!Executor::has("Xtigervnc")) {
        Executor::passthrough("eatmydata apt install -y tigervnc-standalone-server tigervnc-viewer 2>/dev/null || "
            "apt-get install -y tigervnc-standalone-server tigervnc-viewer 2>/dev/null || true");
    }
    // 如果 tigervnc 还是装不上，试试 vnc4server
    if (!Executor::has("Xtigervnc")) {
        Executor::passthrough("eatmydata apt install -y vnc4server 2>/dev/null || "
            "apt-get install -y vnc4server 2>/dev/null || true");
    }

    // 安装 tightvncserver
    if (!Executor::has("Xtightvnc")) {
        Executor::passthrough("eatmydata apt install -y tightvncserver 2>/dev/null || "
            "apt-get install -y tightvncserver 2>/dev/null || true");
        Executor::shell("sed -i -E 's@(configure)@pre\\1@' "
            "/var/lib/dpkg/info/tightvncserver.postinst 2>/dev/null || true");
        Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
    }

    // xfonts
    if (!fs::exists("/usr/share/fonts/X11/100dpi/timR24.pcf.gz")) {
        Executor::passthrough("eatmydata apt install -y xfonts-100dpi 2>/dev/null || "
            "apt-get install -y xfonts-100dpi 2>/dev/null || true");
    }
    if (!fs::exists("/usr/share/fonts/X11/75dpi/term14.pcf.gz")) {
        Executor::passthrough("eatmydata apt install -y xfonts-75dpi 2>/dev/null || "
            "apt-get install -y xfonts-75dpi 2>/dev/null || true");
    }
    if (!fs::exists("/usr/share/fonts/X11/Type1/c0419bt_.afm")) {
        Executor::passthrough("eatmydata apt install -y xfonts-scalable 2>/dev/null || "
            "apt-get install -y xfonts-scalable 2>/dev/null || true");
    }

    // Speedo 字体软链接
    if (fs::exists("/usr/share/fonts/X11/Type1") && !fs::exists("/usr/share/fonts/X11/Speedo")) {
        Executor::shell("ln -svf /usr/share/fonts/X11/Type1 /usr/share/fonts/X11/Speedo 2>/dev/null || true");
    }

    // 写回 VNC_SERVER 到 startvnc
    if (!vnc_config_.server.empty()) {
        Executor::shell("sed -i -E 's@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
    }
}

std::string VncManager::grep_tiger_vnc_deb_file(const std::string &latest_deb_repo,
                                                const std::string &grep_name_01,
                                                const std::string &grep_name_02) {
    // 对应旧 Bash grep_tiger_vnc_deb_file (gui:5346-5350)
    std::string arch;
    auto arch_result = Executor::shell("dpkg --print-architecture 2>/dev/null || uname -m");
    arch = arch_result.ok() ? arch_result.stdout_data : "amd64";
    while (!arch.empty() && (arch.back() == '\n' || arch.back() == '\r')) arch.pop_back();

    auto result = Executor::shell("curl -L '" + latest_deb_repo + "' 2>/dev/null | grep '\\.deb' | "
                                  "grep '" + arch + "' | grep '" + grep_name_01 + "' | grep '" + grep_name_02 +
                                  "' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2");
    std::string version = result.ok() ? result.stdout_data : "";
    while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();
    if (!version.empty()) return latest_deb_repo + version;
    return "";
}

void VncManager::ubuntu_install_tiger_vnc_server() {
    // 对应旧 Bash ubuntu_install_tiger_vnc_server (gui:5352-5376)
    Logger::step("Ubuntu Focal tigervnc 专用安装 (apt-mark hold)...");

    Executor::passthrough("apt-mark unhold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
    debian_install_vnc_server();

    const std::string deb_repo = "https://mirrors.bfsu.edu.cn/debian/pool/main/t/tigervnc/";
    const std::string tmp_dir = "/tmp/.TIGER_VNC_TEMP_FOLDER";
    Executor::shell("mkdir -p " + tmp_dir);

    // 使用 grep_tiger_vnc_deb_file 获取最新 deb URL, 避免内联 curl 管道
    std::string common_url = grep_tiger_vnc_deb_file(deb_repo, "tigervnc-common", "deb10");
    std::string server_url = grep_tiger_vnc_deb_file(deb_repo, "tigervnc-standalone-server", "deb10");
    std::string jpeg_url = grep_tiger_vnc_deb_file(
        "https://mirrors.bfsu.edu.cn/debian/pool/main/libj/libjpeg-turbo/", "libjpeg62-turbo_", "deb");

    if (!common_url.empty())
        Executor::shell("cd " + tmp_dir + " && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-s 5 -x 5 -k 1M -o 'tigervnc-common_ubuntu-focal.deb' '" + common_url + "' 2>/dev/null || "
                        "curl -L -o 'tigervnc-common_ubuntu-focal.deb' '" + common_url + "' 2>/dev/null || true");

    if (!server_url.empty())
        Executor::shell("cd " + tmp_dir + " && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-s 5 -x 5 -k 1M -o 'tigervnc-standalone-server_ubuntu-focal.deb' '" + server_url +
                        "' 2>/dev/null || curl -L -o 'tigervnc-standalone-server_ubuntu-focal.deb' '" + server_url +
                        "' 2>/dev/null || true");

    if (!jpeg_url.empty())
        Executor::shell("cd " + tmp_dir + " && aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                        "-s 5 -x 5 -k 1M -o 'libjpeg62-turbo_ubuntu-focal.deb' '" + jpeg_url +
                        "' 2>/dev/null || curl -L -o 'libjpeg62-turbo_ubuntu-focal.deb' '" + jpeg_url +
                        "' 2>/dev/null || true");

    Executor::passthrough("cd " + tmp_dir + " && "
                          "dpkg -i ./libjpeg62-turbo_ubuntu-focal.deb "
                          "./tigervnc-common_ubuntu-focal.deb "
                          "./tigervnc-standalone-server_ubuntu-focal.deb 2>/dev/null || true");
    Executor::passthrough("apt-mark hold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
    Executor::shell("rm -rvf " + tmp_dir + " 2>/dev/null || true");
}

void VncManager::case_debian_distro_and_install_vnc() {
    // 对应旧 Bash case_debian_distro_and_install_vnc (gui:5407-5430)
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
    if (family != DistroFamily::Debian) return;

    bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
    if (is_ubuntu) {
        auto os_check = Executor::shell("grep -Eq 'Focal Fossa|focal|Eoan Ermine' /etc/os-release && echo 'yes'");
        bool is_focal_like = os_check.ok() && os_check.stdout_data.find("yes") != std::string::npos;
        if (is_focal_like && vnc_config_.server_bin == "tigervnc") {
            // 检查已安装版本
            auto ver_check = Executor::shell("apt list --installed 2>&1 | grep 'tigervnc-standalone-server' | "
                "awk '{print $2}' | grep '1.9.'");
            if (!ver_check.ok() || ver_check.stdout_data.empty()) {
                ubuntu_install_tiger_vnc_server();
            }
        } else if (is_focal_like) {
            Executor::passthrough("apt-mark unhold tigervnc-common tigervnc-standalone-server 2>/dev/null || true");
            debian_install_vnc_server();
        } else {
            debian_install_vnc_server();
        }
    } else {
        debian_install_vnc_server();
    }
    if (!vnc_config_.server.empty()) {
        Executor::shell("sed -i -E 's@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
    }
}

void VncManager::which_vnc_server_do_you_prefer() {
    // 对应旧 Bash which_vnc_server_do_you_prefer (gui:5385-5405)
    // 根据 REMOTE_DESKTOP_SESSION_01 推荐 VNC server
    bool recommend_tiger = false;
    if (Executor::has("startplasma-x11") || Executor::has("startlxqt") ||
        Executor::has("gnome-shell") || Executor::has("cinnamon-session") ||
        Executor::has("startdde") || Executor::has("ukui-session") || Executor::has("budgie-desktop")) {
        recommend_tiger = true;
    }
    choose_vnc_server();
    modify_to_xfwm4_breeze_theme();
    if (!vnc_config_.server.empty()) {
        Executor::shell("sed -i -E 's@^(VNC_SERVER)=.*@\\1=" + vnc_config_.server +
                        "@' /usr/local/bin/startvnc 2>/dev/null || true");
    }
}

void VncManager::modify_to_xfwm4_breeze_theme() {
    // 对应旧 Bash modify_to_xfwm4_breeze_theme (gui:5378-5383)
    if (fs::exists("/usr/share/themes/Breeze/xfwm4/themerc")) {
        Executor::shell(
            "dbus-launch xfconf-query -c xfwm4 -t string -np /general/theme -s Breeze 2>/dev/null || true");
    }
}

void VncManager::create_the_which_script() {
    // 对应旧 Bash create_the_which_script (gui:5768-5779)
    std::string which_file = "/usr/local/bin/which";
    if (!fs::exists(which_file)) {
        Logger::info("创建 /usr/local/bin/which 包装脚本 (command -v)");
        SystemHelper::write_file(fs::path(which_file), "#!/bin/sh\ncommand -v \"$@\"\n");
        Executor::shell("chmod a+rx " + which_file);
    }
}

void VncManager::check_the_which_command() {
    // 对应旧 Bash check_the_which_command (gui:5780-5791)
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();
    if (family == DistroFamily::Debian) {
        create_the_which_script();
    } else {
        if (!fs::exists("/usr/bin/which")) create_the_which_script();
    }
}

void VncManager::if_container_is_arm() {
    // 对应旧 Bash if_container_is_arm (gui:5745-5760)
    auto arch_result = Executor::shell("dpkg --print-architecture 2>/dev/null || uname -m");
    std::string arch = arch_result.ok() ? arch_result.stdout_data : "";
    while (!arch.empty() && (arch.back() == '\n' || arch.back() == '\r')) arch.pop_back();

    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown) family = PackageManager::detect_distro_family();

    if (family == DistroFamily::RedHat) {
        // RedHat: arm* 架构跳过
        if (arch.rfind("arm", 0) != 0) {
            // install_novnc(); -- 注意：此方法在 GUIManager 中，此处无法调用
            // 保留原逻辑的占位，实际调用由 GUIManager 协调
        }
    } else {
        // 其他: 仅 armhf/armel 跳过
        if (arch != "armhf" && arch != "armel") {
            // install_novnc(); -- 同上的说明
        }
    }
}

void VncManager::auto_select_keyboard_layout() {
    // 对应旧 Bash auto_select_keyboard_layout (gui:756-760)
    Executor::shell("printf '%s\\n' 'debconf debconf/frontend select Noninteractive' | "
        "debconf-set-selections 2>/dev/null || true");
    Executor::shell(
        "printf '%s\\n' \"keyboard-configuration keyboard-configuration/layout select 'English (US)'\" | "
        "debconf-set-selections 2>/dev/null || true");
    Executor::shell("echo keyboard-configuration keyboard-configuration/layoutcode select 'us' | "
        "debconf-set-selections 2>/dev/null || true");
}

void VncManager::fix_mlocate() {
    // 对应旧 Bash fix_mlocate + get_ubuntu_version (gui:1624-1652)
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (family == DistroFamily::Unknown)
        family = PackageManager::detect_distro_family();
    if (family != DistroFamily::Debian) return;
    if (cfg_.sub_distro != "ubuntu") return;

    // 检查 ubuntu 版本
    auto ver_check = Executor::shell(
        "grep -qE 'Focal Fossa|focal|bionic|Bionic Beaver|xenial|Xenial Xerus|impish|Impish Indri' "
        "/etc/os-release && echo 'old' || echo 'new'");
    if (!ver_check.ok() || ver_check.stdout_data.find("old") == std::string::npos) return;

    // 修复 mlocate postinst (老旧 ubuntu 版本)
    Logger::info("修复 mlocate postinst (老旧 Ubuntu 版本)...");
    std::string postinst_src = "/usr/local/etc/tmoe-linux/git/share/old-version/tools/gui/config/mlocate.postinst";
    std::string postinst_dst = "/var/lib/dpkg/info/mlocate.postinst";
    if (fs::exists(postinst_src)) {
        Executor::shell("cp -f " + postinst_src + " " + postinst_dst + " 2>/dev/null || true");
        Logger::ok("mlocate postinst 已修复");
    } else {
        // 备选：直接修复
        Executor::shell("sed -i 's@set -e@set -e\\nexit 0@' " + postinst_dst + " 2>/dev/null || true");
    }
}

bool VncManager::choose_vnc_server() {
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

bool VncManager::configure_vnc_password(std::string_view password) {
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
        passwd_to_use = Executor::tui_select(pass_cmd);
        // tui_select 已去除尾部换行

        if (passwd_to_use.empty()) {
            Logger::warn(_("gui.vnc.no_password"));
            return true;
        }
    }

    // check_vnc_passsword_length: 验证密码长度 (VNC 密码必须为 6-8 位)
    if (passwd_to_use.length() < 6 || passwd_to_use.length() > 8) {
        std::string msg = (passwd_to_use.length() < 6)
                              ? "密码长度太短，至少需要6位字符\\nPassword too short, at least 6 characters required"
                              : "密码长度太长，最多允许8位字符\\nPassword too long, at most 8 characters allowed";
        Logger::warn(msg);
        Executor::passthrough(cfg_.tui_bin +
                              " --title \"密码长度错误 / Password Length Error\""
                              " --msgbox \"" + msg + "\" 10 50");
        if (password.empty()) {
            // 交互模式：重新提示
            return configure_vnc_password("");
        } else {
            // 非交互模式：使用安全默认值
            Logger::info("使用默认密码 tmoe123");
            passwd_to_use = "tmoe123";
        }
    }

    // 写入密码文件 (使用 vncpasswd 或 x11vnc -storepasswd 作为 fallback)
    std::string cmd;
    if (Executor::has("vncpasswd")) {
        cmd = "echo '" + passwd_to_use + "' | vncpasswd -f > " +
              vnc_config_.passwd_file.string() + " 2>/dev/null";
    } else {
        cmd = "x11vnc -storepasswd " + passwd_to_use + " " + vnc_config_.passwd_file.string() + " 2>/dev/null";
    }

    if (Executor::passthrough(cmd).ok()) {
        vnc_config_.password = passwd_to_use;
        // 修复权限 (仅 root)
        if (cfg_.is_root) {
            Executor::shell("chmod 600 " + vnc_config_.passwd_file.string() + " 2>/dev/null");
        }
        Logger::ok(_f("gui.vnc.password_set", vnc_config_.passwd_file.string()));
        // 对应旧 Bash: cp passwd x11passwd; chmod 600 x11passwd
        if (fs::exists(vnc_config_.passwd_file)) {
            fs::path x11passwd = vnc_config_.vnc_home_dir / "x11passwd";
            Executor::shell(
                "cp " + vnc_config_.passwd_file.string() + " " + x11passwd.string() + " 2>/dev/null || true");
            Executor::shell("chmod 600 " + x11passwd.string() + " 2>/dev/null || true");
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

    return SystemHelper::write_file(vnc_config_.tigervnc_config, config.str());
}

// ═══════════════════════════════════════════════════════════════
// 配置内容生成 (桌面会话脚本)
// ═══════════════════════════════════════════════════════════════

// 简单的桌面名称 → 会话命令映射 (用于 VncManager 内部，不依赖 GUIManager 的桌面注册表)
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
        return {"xfce4-session", "startxfce4"}; // 默认
    }
}

std::string VncManager::generate_xsession_content(std::string_view desktop) const {
    SessionCmds cmds = lookup_session_commands(desktop);

    std::ostringstream script;
    script << "#!/bin/bash\n"
            << "# tmoe-linux Xsession — 自动生成，请勿手动修改\n"
            << "# 解除 WSLg 环境变量冲突 (对应旧 Bash startvnc:22)\n"
            << "unset WAYLAND_DISPLAY\n"
            << "unset XDG_RUNTIME_DIR\n"
            << "# WSL/xRDP 无物理 GPU，强制软件渲染避免 OpenGL 崩溃\n"
            << "export LIBGL_ALWAYS_SOFTWARE=1\n"
            << "export GALLIUM_DRIVER=llvmpipe\n\n"
            << "SESSION_01=\"" << cmds.cmd1 << "\"\n"
            << "SESSION_02=\"" << cmds.cmd2 << "\"\n\n"
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
            << "    [[ ! -s /etc/environment ]] || source /etc/environment\n"
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
            << "    exec dbus-launch --exit-with-session $REMOTE_DESKTOP_SESSION \"$@\"\n"
            << "}\n\n"
            << "start_session\n";

    return script.str();
}

std::string VncManager::generate_xstartup_content() const {
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

bool VncManager::configure_xstartup(std::string_view desktop) {
    // 对应旧 Bash: configure_vnc_xstartup() + first_configure_startvnc()
    Logger::step(_f("gui.vnc.configuring_xsession", std::string(desktop)));

    // 1. 创建 Xsession
    std::string xsession_content = generate_xsession_content(desktop);
    if (!SystemHelper::write_file(vnc_config_.xsession_file, xsession_content)) {
        Logger::error(_f("gui.vnc.xsession_write_failed", vnc_config_.xsession_file.string()));
        return false;
    }
    Executor::shell("chmod 777 " + vnc_config_.xsession_file.string()); // 对应旧 Bash chmod 777

    // 2. 确保目录和 machine-id
    Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");
    Executor::shell("ln -svf /run /var/ 2>/dev/null || true"); // 对应旧 Bash configure_vnc_xstartup
    if (!fs::exists("/etc/machine-id")) {
        Executor::shell("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
            "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
    }
    if (!fs::exists("/etc/machine-id") || fs::file_size("/etc/machine-id") == 0) {
        SystemHelper::write_file("/etc/machine-id", "0ecb780817003d3342d16adb5ff1dfa9\n");
    }
    if (!fs::exists("/var/lib/dbus/machine-id")) {
        Executor::shell("ln -svf /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
    }

    // 3. 创建 ~/.vnc/xstartup
    Executor::shell("mkdir -p ~/.vnc /etc/X11/xinit /etc/tigervnc 2>/dev/null");
    std::string xstartup_content = generate_xstartup_content();
    if (!SystemHelper::write_file(vnc_config_.xstartup_file, xstartup_content)) {
        Logger::error(_f("gui.vnc.xstartup_write_failed", vnc_config_.xstartup_file.string()));
        return false;
    }
    Executor::shell("chmod +x " + vnc_config_.xstartup_file.string());
    // 旧 Bash: ln -svf ${XSESSION_FILE} ./xstartup
    Executor::shell("ln -sf " + vnc_config_.xsession_file.string() + " " +
                    vnc_config_.xstartup_file.string() + " 2>/dev/null || true");

    // 4. configure_xvnc: tigervnc 默认配置
    configure_vnc_defaults();

    // 注意：first_configure_vnc 不在这里调用，由 GUIManager 协调

    Logger::ok(_("gui.vnc.xsession_configured"));
    return true;
}

// ═══════════════════════════════════════════════════════════════
// VNC 命令构建
// ═══════════════════════════════════════════════════════════════

std::string VncManager::build_vnc_start_command(int display, int width, int height) const {
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

std::string VncManager::build_xvfb_command(int display, int width, int height) const {
    if (display <= 0) display = 233; // x11vnc 默认使用 233
    if (width <= 0) width = vnc_config_.resolution_w;
    if (height <= 0) height = vnc_config_.resolution_h;

    std::ostringstream cmd;
    cmd << "Xvfb :" << display
            << " -screen 0 " << width << "x" << height << "x24"
            << " -ac +extension RANDR &";
    return cmd.str();
}

std::string VncManager::build_x11vnc_command(int display) const {
    if (display <= 0) display = 233;

    std::ostringstream cmd;
    cmd << "x11vnc -display :" << display
            << " -ncache_cr -xkb -noxrecord -noxdamage"
            << " -forever -bg -noshm -cursor arrow"
            << " -rfbport " << vnc_config_.rfb_port;

    // 对应旧 Bash: x11vnc 使用独立的 x11passwd 文件
    fs::path x11_passwd = vnc_config_.vnc_home_dir / "x11passwd";
    if (fs::exists(x11_passwd)) {
        cmd << " -rfbauth " << x11_passwd.string();
    } else if (!vnc_config_.password.empty()) {
        cmd << " -rfbauth " << vnc_config_.passwd_file.string();
    }

    return cmd.str();
}

// ═══════════════════════════════════════════════════════════════
// VNC 启动/停止
// ═══════════════════════════════════════════════════════════════

bool VncManager::start_vnc(int display, int width, int height) {
    Logger::step(_("gui.vnc.start"));

    if (display > 0) {
        vnc_config_.display = display;
        vnc_config_.update_port();
    }
    if (width > 0) vnc_config_.resolution_w = width;
    if (height > 0) vnc_config_.resolution_h = height;

    // 检查 Xvnc
    if (!check_xvnc_command()) {
        Logger::error(_("gui.vnc.server_not_available"));
        return false;
    }

    // 确保 xstartup 存在
    if (!fs::exists(vnc_config_.xstartup_file)) {
        Logger::warn(_("gui.vnc.xstartup_missing"));
        configure_xstartup("xfce");
    }

    // 如果已经运行则提示
    if (is_vnc_running()) {
        Logger::warn(_f("gui.vnc.already_running", std::to_string(vnc_config_.display)));
        return true;
    }

    // 清理残留锁文件
    std::string lock_path = "/tmp/.X" + std::to_string(vnc_config_.display) + "-lock";
    std::string socket_path = "/tmp/.X11-unix/X" + std::to_string(vnc_config_.display);
    Executor::shell("rm -f " + lock_path + " " + socket_path + " 2>/dev/null");

    // WSL/WSLg 冲突处理 (对应旧 Bash startvnc:22 unset WAYLAND_DISPLAY)
    // WSLg 设置了 WAYLAND_DISPLAY 环境变量，会导致 VNC/xRDP 闪退
    std::string env_prefix;
    if (cfg_.is_wsl) {
        detect_wsl_environment();
        env_prefix = "unset WAYLAND_DISPLAY; ";
        // WSL2: PulseAudio 指向 Windows 宿主机 IP
        if (!vnc_config_.windows_ip.empty() && vnc_config_.windows_ip != "127.0.0.1") {
            env_prefix += "export PULSE_SERVER=" + vnc_config_.windows_ip + "; ";
            Logger::info("WSL2 PulseAudio 服务器: " + vnc_config_.windows_ip);
        }
    }

    // 启动 D-Bus (PulseAudio 桥接由 GUIManager 负责，此处只启动 dbus)
    launch_dbus_daemon();

    // 设置环境变量并启动 VNC (WSL 下前缀 unset WAYLAND_DISPLAY 避免 WSLg 冲突)
    std::string cmd = build_vnc_start_command(display, width, height);
    ExecResult result = Executor::passthrough(env_prefix + cmd + " > /tmp/tmoe_vnc_startup.log 2>&1");

    if (result.ok()) {
        Logger::ok(_f("gui.vnc.started",
                      std::to_string(vnc_config_.rfb_port) + " (display :" +
                      std::to_string(vnc_config_.display) + ")"));
        Logger::info(_f("gui.vnc.resolution_info",
                        std::to_string(vnc_config_.resolution_w) + "x" + std::to_string(vnc_config_.resolution_h)));
        Logger::info(_f("gui.vnc.connection_address", get_vnc_connection_uri()));

        // 显示局域网地址
        std::string ips = get_local_ip_addresses();
        if (!ips.empty()) {
            Logger::info(_f("gui.vnc.lan_address", ips));
        }

        // 写入 PID 文件
        write_vnc_pid_file(vnc_config_.display);

        return true;
    }

    Logger::error(_("gui.vnc.start_failed"));
    return false;
}

bool VncManager::stop_vnc(int display) {
    Logger::step(_("gui.vnc.stop"));

    if (display <= 0) display = vnc_config_.display;

    // 1. 使用 vncserver -kill 清理 (TigerVNC)
    Executor::passthrough("vncserver -kill :" + std::to_string(display) + " 2>/dev/null");

    // 2. 基于 PID 文件精确停止
    remove_vnc_pid_file(display);

    // 3. pkill
    Executor::shell("pkill -f 'Xvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
    Executor::shell("pkill -f 'Xtigervnc.*:" + std::to_string(display) + "' 2>/dev/null || true");
    Executor::shell("pkill -f 'Xtightvnc.*:" + std::to_string(display) + "' 2>/dev/null || true");

    // 4. 清理锁文件
    Executor::shell("rm -f /tmp/.X" + std::to_string(display) + "-lock 2>/dev/null");
    Executor::shell("rm -f /tmp/.X11-unix/X" + std::to_string(display) + " 2>/dev/null");

    // 5. 停止 websockify (noVNC)
    Executor::shell("pkill -f 'websockify.*:" + std::to_string(vnc_config_.rfb_port) + "' 2>/dev/null || true");

    // 6. 对应旧 Bash stopvnc 的 pkill_all_vnc: 残余进程彻底清理
    kill_all_vnc();

    Logger::ok(_f("gui.vnc.stopped", std::to_string(display)));
    return true;
}

bool VncManager::start_x11vnc(int display) {
    Logger::step(_("gui.x11vnc.starting"));

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
        Logger::ok(_f("gui.x11vnc.started", std::to_string(vnc_config_.rfb_port)));
        return true;
    }

    Logger::error(_("gui.x11vnc.start_failed"));
    return false;
}

bool VncManager::stop_x11vnc() {
    Logger::step(_("gui.x11vnc.stopping"));
    Executor::shell("pkill -f x11vnc 2>/dev/null || true");
    Executor::shell("pkill Xvfb 2>/dev/null || true");
    Executor::shell("rm -f /tmp/.X233-lock /tmp/.X11-unix/X233 2>/dev/null || true");
    Logger::ok(_("gui.x11vnc.stopped"));
    return true;
}

bool VncManager::kill_all_vnc() {
    Logger::step(_("gui.vnc.killing_all"));
    Executor::shell("pkill Xtightvnc 2>/dev/null || true");
    Executor::shell("pkill Xtigervnc 2>/dev/null || true");
    Executor::shell("pkill Xvnc 2>/dev/null || true");
    Executor::shell("pkill x11vnc 2>/dev/null || true");
    Executor::shell("pkill Xvfb 2>/dev/null || true");

    // 清理所有锁文件
    Executor::shell("rm -f /tmp/.X*-lock /tmp/.X11-unix/X* 2>/dev/null || true");
    Logger::ok(_("gui.vnc.killed_all"));
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 运行时状态查询
// ═══════════════════════════════════════════════════════════════

bool VncManager::is_vnc_running(int display) const {
    if (display <= 0) display = vnc_config_.display;

    // 检查进程
    auto result = Executor::shell("pgrep -f 'X(vnc|tigervnc|tightvnc).*:" +
                                  std::to_string(display) + "' 2>/dev/null");
    if (result.ok() && !result.stdout_data.empty()) return true;

    // 检查锁文件
    if (fs::exists("/tmp/.X" + std::to_string(display) + "-lock")) return true;

    return false;
}

int VncManager::detect_available_display() const {
    for (int d = 1; d <= 10; ++d) {
        if (!fs::exists("/tmp/.X" + std::to_string(d) + "-lock")) {
            return d;
        }
    }
    return 1;
}

std::string VncManager::get_vnc_connection_uri() const {
    return "vnc://127.0.0.1:" + std::to_string(vnc_config_.rfb_port);
}

std::string VncManager::get_local_ip_addresses() const {
    std::ostringstream ips;
    // IPv4
    auto v4 = Executor::shell(
        "ip -4 addr show scope global | grep inet | awk '{print $2}' | cut -d/ -f1 2>/dev/null");
    if (v4.ok() && !v4.stdout_data.empty()) {
        std::string data = v4.stdout_data;
        std::istringstream iss(data);
        std::string line;
        while (std::getline(iss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
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

// ═══════════════════════════════════════════════════════════════
// PID 管理
// ═══════════════════════════════════════════════════════════════

void VncManager::write_vnc_pid_file(int display) const {
    std::ostringstream cmd;
    cmd << "pgrep -f 'X(vnc|tigervnc|tightvnc).*:" << display << "' | head -1 > "
            << vnc_config_.vnc_pid_file.string() << " 2>/dev/null";
    Executor::shell(cmd.str());
    // 也写入 x.pid (TigerVNC 兼容)
    Executor::shell("pgrep -f 'X(tigervnc|tightvnc).*:" + std::to_string(display) + "' | head -1 > "
                    + vnc_config_.x_pid_file.string() + " 2>/dev/null || true");
}

void VncManager::remove_vnc_pid_file(int display) const {
    // 基于 PID 文件杀进程
    if (fs::exists(vnc_config_.vnc_pid_file)) {
        auto pid_data = SystemHelper::read_file(vnc_config_.vnc_pid_file);
        if (!pid_data.empty()) {
            std::string mypid = pid_data;
            while (!mypid.empty() && (mypid.back() == '\n' || mypid.back() == '\r')) mypid.pop_back();
            if (!mypid.empty())
                Executor::passthrough("kill " + mypid + " 2>/dev/null || true");
        }
        fs::remove(vnc_config_.vnc_pid_file);
    }
    if (fs::exists(vnc_config_.x_pid_file)) {
        auto pid_data = SystemHelper::read_file(vnc_config_.x_pid_file);
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
// 环境检测
// ═══════════════════════════════════════════════════════════════

bool VncManager::detect_android_resolution(int &width, int &height) const {
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

void VncManager::detect_wsl_environment() {
    // WSL1/WSL2 检测: uname -r 第二字段为 "microsoft" 则是 WSL2
    auto wsl_field = Executor::shell("uname -r | cut -d '-' -f 2 2>/dev/null");
    std::string field2 = wsl_field.ok() ? wsl_field.stdout_data : "";
    while (!field2.empty() && (field2.back() == '\n' || field2.back() == '\r')) field2.pop_back();
    if (field2 == "microsoft") {
        // WSL2: get gateway IP from ip route
        auto route = Executor::shell(
            "ip route list table 0 | head -n 1 | awk -F 'default via ' '{print $2}' | awk '{print $1}' 2>/dev/null");
        if (route.ok() && !route.stdout_data.empty()) {
            std::string ip = route.stdout_data;
            while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r')) ip.pop_back();
            vnc_config_.windows_ip = ip;
        }
        Logger::info("检测到 WSL2, 网关 IP: " + vnc_config_.windows_ip);
    } else {
        vnc_config_.windows_ip = "127.0.0.1";
        Logger::info("检测到 WSL1: DISPLAY=localhost:0");
    }
}

// ═══════════════════════════════════════════════════════════════
// 权限 / D-Bus / 脚本部署
// ═══════════════════════════════════════════════════════════════

bool VncManager::fix_vnc_permissions() {
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

bool VncManager::deploy_startup_scripts() {
    Logger::step("部署启动脚本到 /usr/local/bin...");

    // 方案B: thin wrappers — 所有逻辑在C++, bash只做跳板
    // 用户打 startvnc → bash script → exec tmoe gui --start-vnc → C++ VncManager::start_vnc()

    const char *tmoe_bin = R"(/usr/local/bin/tmoe)";
    if (!fs::exists(tmoe_bin)) {
        // 回退: tmoe 不在PATH
        Logger::warn("未找到 /usr/local/bin/tmoe, 使用直接 C++ 启动逻辑");
    }

    // ── startvnc ──
    std::string startvnc = "#!/bin/bash\n# tmoe-linux startvnc — thin wrapper\n"
                           "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                           "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-vnc \"$@\"\n"
                           "# Fallback: direct start (shouldn't reach here normally)\n"
                           "echo 'Please install tmoe: ln -s $(which tmoe) /usr/local/bin/tmoe'\n";
    SystemHelper::write_file("/usr/local/bin/startvnc", startvnc);
    Executor::shell("chmod +x /usr/local/bin/startvnc");

    // ── stopvnc ──
    std::string stopvnc = "#!/bin/bash\n# tmoe-linux stopvnc — thin wrapper\n"
                          "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                          "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --stop-vnc \"$@\"\n"
                          "# Fallback: pkill all vnc\n"
                          "pkill Xvnc; pkill Xtigervnc; pkill Xtightvnc; pkill x11vnc; pkill websockify 2>/dev/null\n"
                          "echo 'VNC stopped (fallback)'\n";
    SystemHelper::write_file("/usr/local/bin/stopvnc", stopvnc);
    Executor::shell("chmod +x /usr/local/bin/stopvnc");

    // ── startxsdl ──
    std::string startxsdl = "#!/bin/bash\n# tmoe-linux startxsdl — thin wrapper\n"
                            "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                            "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-xsdl \"$@\"\n"
                            "# Fallback: set DISPLAY and exec\n"
                            "export DISPLAY=\"${1:-127.0.0.1:0}\"\n"
                            "export PULSE_SERVER=\"tcp:${DISPLAY%:*}:4713\"\n"
                            "echo \"XSDL DISPLAY=$DISPLAY PULSE_SERVER=$PULSE_SERVER\"\n";
    SystemHelper::write_file("/usr/local/bin/startxsdl", startxsdl);
    Executor::shell("chmod +x /usr/local/bin/startxsdl");

    // ── startx11vnc ──
    std::string startx11vnc = "#!/bin/bash\n# tmoe-linux startx11vnc — thin wrapper\n"
                              "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                              "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-x11vnc \"$@\"\n"
                              "# Fallback: direct start\n"
                              "Xvfb :233 -screen 0 1440x720x24 -ac +extension RANDR &\n"
                              "sleep 2; x11vnc -display :233 -rfbport 5902 -forever -shared -nopw -bg\n"
                              "echo 'x11vnc started on port 5902 (fallback)'\n";
    SystemHelper::write_file("/usr/local/bin/startx11vnc", startx11vnc);
    Executor::shell("chmod +x /usr/local/bin/startx11vnc");

    // ── novnc ──
    std::string novnc = "#!/bin/bash\n# tmoe-linux novnc — thin wrapper\n"
                        "TMOE_BIN=\"" + std::string(tmoe_bin) + "\"\n"
                        "[ -x \"$TMOE_BIN\" ] && exec \"$TMOE_BIN\" gui --start-novnc \"$@\"\n"
                        "# Fallback: direct websockify\n"
                        "NOVNC_DIR=/usr/share/novnc; [ -d \"$NOVNC_DIR\" ] || NOVNC_DIR=/opt/novnc\n"
                        "websockify --web=\"$NOVNC_DIR\" \"${1:-36080}\" localhost:${2:-5902} &\n"
                        "echo \"noVNC: http://localhost:${1:-36080}/vnc.html (fallback)\"\n";
    SystemHelper::write_file("/usr/local/bin/novnc", novnc);
    Executor::shell("chmod +x /usr/local/bin/novnc");

    // ── tightvnc / tigervnc symlinks ──
    Executor::shell("ln -sf /usr/local/bin/startvnc /usr/local/bin/tightvnc 2>/dev/null || true");
    Executor::shell("ln -sf /usr/local/bin/startvnc /usr/local/bin/tigervnc 2>/dev/null || true");

    Logger::ok("启动脚本已部署 (thin wrappers): startvnc, stopvnc, startxsdl, startx11vnc, novnc");
    return true;
}

bool VncManager::launch_dbus_daemon() {
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

bool VncManager::fix_vnc_dbus() {
    Logger::step("修复 VNC dbus-launch...");

    // 确保 dbus 目录和 machine-id 正确
    Executor::shell("mkdir -p /run/dbus /var/lib/dbus 2>/dev/null");

    if (!fs::exists("/etc/machine-id")) {
        Executor::shell("dbus-uuidgen > /etc/machine-id 2>/dev/null || "
            "cat /proc/sys/kernel/random/uuid > /etc/machine-id 2>/dev/null || true");
    }
    if (!fs::exists("/etc/machine-id") || fs::file_size("/etc/machine-id") == 0) {
        SystemHelper::write_file("/etc/machine-id", "0ecb780817003d3342d16adb5ff1dfa9\n");
    }
    if (!fs::exists("/var/lib/dbus/machine-id")) {
        Executor::shell("ln -svf /etc/machine-id /var/lib/dbus/machine-id 2>/dev/null || true");
    }

    Logger::ok("dbus 修复完成");
    return true;
}

bool VncManager::stop_dbus_daemon() {
    Logger::debug("停止 D-Bus 守护进程...");

    // 从 PID 文件精确停止
    if (fs::exists("/run/dbus/pid")) {
        auto pid_data = SystemHelper::read_file("/run/dbus/pid");
        if (!pid_data.empty()) {
            Executor::passthrough("kill " + pid_data + " 2>/dev/null || true");
        }
        fs::remove("/run/dbus/pid");
    }
    if (fs::exists("/var/run/dbus/pid")) {
        auto pid_data = SystemHelper::read_file("/var/run/dbus/pid");
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

void VncManager::show_dbus_status() const {
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
// 配置脚本
// ═══════════════════════════════════════════════════════════════

void VncManager::configure_startvnc() {
    // 对应旧 Bash configure_startvnc (gui:5263-5269)
    // 部署 startvnc/stopvnc/tightvnc/tigervnc 脚本
    deploy_startup_scripts();
    Logger::ok("startvnc/stopvnc/tightvnc/tigervnc 已配置");
}

void VncManager::configure_startxsdl() {
    // 对应旧 Bash configure_startxsdl (gui:5241-5261)
    // deploy_startup_scripts 已包含 startxsdl，这里是额外处理
    bool is_wsl = cfg_.is_wsl;
    if (is_wsl) {
        // 复制 wslg 脚本
        Executor::shell("cp -af /usr/local/etc/tmoe-linux/git/share/old-version/tools/gui/wslg "
            "/usr/local/bin/wslg 2>/dev/null || true");
    }
    // centos 禁用 dbus autostart
    if (cfg_.sub_distro == "centos") {
        Executor::shell("sed -i -E 's@(AUTO_START_DBUS=).*@\\1false@' "
            "/usr/local/bin/startvnc /usr/local/bin/startxsdl 2>/dev/null || true");
    }
}

// ═══════════════════════════════════════════════════════════════
// 权限修复
// ═══════════════════════════════════════════════════════════════

void VncManager::fix_non_root_permissions() {
    // 对应旧 Bash fix_non_root_permissions (gui:5271-5276)
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    if (home != "/root") {
        Logger::info("修复非 root 用户权限: " + home);
        std::string user = Executor::shell("id -un").stdout_data;
        std::string group = Executor::shell("id -gn").stdout_data;
        while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
        while (!group.empty() && (group.back() == '\n' || group.back() == '\r')) group.pop_back();
        Executor::shell("chown -R " + user + ":" + group + " " + home +
                        "/.vnc " + home + "/.Xauthority " + home + "/.ICEauthority "
                        + home + "/.config " + home + "/.cache " + home + "/.dbus " + home + "/.local "
                        "2>/dev/null || true");
    }
}

// ═══════════════════════════════════════════════════════════════
// VNC 配置修改
// ═══════════════════════════════════════════════════════════════

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
        Logger::warn("未检测到startvnc,您可能尚未安装图形桌面");
    }
}

// ═══════════════════════════════════════════════════════════════
// x11vnc 配置修改 (TUI 子菜单使用)
// ═══════════════════════════════════════════════════════════════

void VncManager::modify_x11vnc_pulse_server() {
    std::string cmd = cfg_.tui_bin +
                      " --title \"MODIFY PULSE SERVER ADDRESS\""
                      " --inputbox \"输入 PulseAudio 服务器地址\\\\n例如 127.0.0.1 或 192.168.1.3:4713\" 15 50";
    std::string addr = Executor::tui_select(cmd);
    if (!addr.empty())
        Executor::shell(
            "sed -i 's@PULSE_SERVER=.*@PULSE_SERVER=" + addr + "@' /usr/local/bin/startx11vnc 2>/dev/null || true");
}

void VncManager::modify_x11vnc_resolution() {
    std::string cmd = cfg_.tui_bin +
                      " --title \"resolution\""
                      " --inputbox \"输入分辨率 (例如 1280x720)\" 10 50 \"1280x720\"";
    std::string res = Executor::tui_select(cmd);
    if (!res.empty())
        Executor::shell(
            "sed -i 's@-geometry .* @-geometry " + res + " @' /usr/local/bin/startx11vnc 2>/dev/null || true");
}

void VncManager::modify_x11vnc_port() {
    std::string cmd = cfg_.tui_bin +
                      " --title \"port\""
                      " --inputbox \"输入 x11vnc TCP 端口\\\\n默认 5902\" 10 50 \"5902\"";
    std::string port = Executor::tui_select(cmd);
    if (!port.empty())
        Executor::shell(
            "sed -i 's@-rfbport .* @-rfbport " + port + " @' /usr/local/bin/startx11vnc 2>/dev/null || true");
}

} // namespace tmoe::domain
