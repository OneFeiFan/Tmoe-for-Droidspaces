#include "domain/apps/dev_tools.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    DeveloperTools::DeveloperTools(const TmoeConfig &cfg) : cfg_(cfg) {
        apps_lnk_dir_ = fs::path("/usr/share/applications");
        download_path_ = fs::path("/tmp/tmoe-downloads");
    }

    void DeveloperTools::reset_state() {
        community_edition_ = false;
        dev_menu_type_ = 0;
        grep_name_.clear();
        lnk_file_.clear();
        bin_file_.clear();
        icon_name_.clear();
        app_opt_dir_.clear();
        icon_file_.clear();
    }

    // ═══════════════════════════════════════════════════════════════════
    // 主入口: development_programming_tools() — 11项IDE菜单
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::run_dev_tools_menu() {
        while (true) {
            reset_state();

            std::string tip = _("devtools.tightvnc_tip");
            std::string menu = cfg_.tui_bin + " --title \"" + _("devtools.menu_title") + "\""
                               " --menu \"" + tip + "\" 0 50 0 "
                               "\"1\"  \"🇻 🇸 Visual Studio Code (现代化代码编辑器)\" "
                               "\"2\"  \"🇦 🇸 Android Studio (Google推出的安卓IDE)\" "
                               "\"3\"  \"🇮 🇯 IntelliJ IDEA (Java集成开发环境)\" "
                               "\"4\"  \"🇵 🇨 PyCharm (Python集成开发环境)\" "
                               "\"5\"  \"🇼 🇸 WebStorm (JavaScript IDE,Web前端开发工具)\" "
                               "\"6\"  \"🇨 🇱 CLion (C/C++ IDE)\" "
                               "\"7\"  \"🇬 🇴 GoLand (Golang IDE)\" "
                               "\"8\"  \"GNU Emacs (可扩展,可定制,支持自文档化)\" "
                               "\"9\"  \"Code::Blocks (C,C++和Fortran的IDE)\" "
                               "\"10\" \"GitHub Desktop (x64,GitHub官方桌面客户端)\" "
                               "\"11\" \"Sublime Text (x64,漂亮的UI,非凡的功能)\" "
                               "\"0\"  \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            switch (std::stoi(ch)) {
                case 1: // VS Code → source vscode script → which_vscode_edition
                    run_vscode_menu();
                    break;

                case 2: // Android Studio → dev_menu_02
                    prep_android_studio();
                    run_ide_submenu_02();
                    break;

                case 3: // IntelliJ IDEA → 选择旗舰/社区版
                    choose_idea_edition();
                    run_ide_submenu_01();
                    break;

                case 4: // PyCharm Community
                    community_edition_ = true;
                    grep_name_ = "pycharm-community-edition";
                    lnk_file_ = "pycharm-community-edition.desktop";
                    bin_file_ = "/opt/pycharm-community-edition/bin/pycharm.sh";
                    icon_name_ = "pycharm-community-edition.png";
                    app_opt_dir_ = "/opt/pycharm-community-edition";
                    run_ide_submenu_01();
                    break;

                case 5: // WebStorm
                    grep_name_ = "webstorm";
                    lnk_file_ = "webstorm.desktop";
                    bin_file_ = "/opt/webstorm/bin/webstorm.sh";
                    icon_name_ = "webstorm.png";
                    app_opt_dir_ = "/opt/webstorm";
                    run_ide_submenu_01();
                    break;

                case 6: // CLion
                    grep_name_ = "clion-1";
                    lnk_file_ = "clion-1.desktop";
                    bin_file_ = "/opt/clion-1/bin/clion.sh";
                    icon_name_ = "clion-1.png";
                    app_opt_dir_ = "/opt/clion-1";
                    run_ide_submenu_01();
                    break;

                case 7: // GoLand
                    grep_name_ = "goland";
                    lnk_file_ = "goland.desktop";
                    bin_file_ = "/opt/goland/bin/goland.sh";
                    icon_name_ = "goland.png";
                    app_opt_dir_ = "/opt/goland";
                    run_ide_submenu_01();
                    break;

                case 8: // GNU Emacs
                    install_emacs();
                    break;

                case 9: // Code::Blocks
                    install_code_blocks();
                    break;

                case 10: // GitHub Desktop
                    grep_name_ = "github-desktop-bin";
                    lnk_file_ = "github-desktop.desktop";
                    bin_file_ = "/usr/bin/github-desktop";
                    icon_name_ = "github-desktop.png";
                    app_opt_dir_ = "/opt/github-desktop";
                    run_ide_submenu_01();
                    break;

                case 11: // Sublime Text
                    install_sublime_text();
                    break;
            }

            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // VS Code 子菜单: which_vscode_edition() — 4项
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::run_vscode_menu() {
        while (true) {
            std::string tip = _("devtools.vscode_tip");
            std::string menu = cfg_.tui_bin + " --title \"Visual Studio Code\""
                               " --menu \"" + tip + "\" 0 50 0 "
                               "\"1\" \"Microsoft Official (x64,arm64,armhf官方版)\" "
                               "\"2\" \"VS Code Server (Web版,含配置选项)\" "
                               "\"3\" \"VS Codium (不跟踪你的使用数据)\" "
                               "\"4\" \"修复 tightvnc 无法打开 vscode\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") install_vscode_official();
            else if (ch == "2") run_vscode_server_menu();
            else if (ch == "3") install_vscodium();
            else if (ch == "4") fix_tightvnc_vscode();

            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // VS Code Official: install_vscode_official()
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_vscode_official() {
        std::string arch = cfg_.arch;
        std::string distro = cfg_.linux_distro;
        auto family = infer_family_from_config(distro);

        // Check if already installed
        if (fs::exists("/usr/share/code/.electron")) {
            Logger::info(_("devtools.status.vscode_tar_detected"));
            Logger::info(_("devtools.hint.vscode_launch"));
            Logger::info(_("devtools.hint.vscode_uninstall"));
        } else if (fs::exists("/usr/bin/code")) {
            Logger::info(_("devtools.status.vscode_pkg_detected"));
            Logger::info(_("devtools.hint.vscode_launch"));
        }

        // Try running code if available
        if (Executor::has("code")) {
            Executor::shell("code --version --user-data-dir=/tmp/.code 2>/dev/null || true");
        }

        Logger::step(_("devtools.step.prepare_download_vscode"));
        if (!confirm("devtools.confirm_download_vscode", "是否下载最新版安装包?"))
            return;

        // Install gnome-keyring
        auto family_detected = PackageManager::detect_distro_family();
        PackageManager::install("gnome-keyring", family_detected);

        std::string code_bin_url;
        std::string code_bin_folder;

        // Select download URL based on arch + distro
        if (arch == "amd64") {
            if (family == DistroFamily::Debian)
                code_bin_url = "https://go.microsoft.com/fwlink/?LinkID=760868";
            else if (family == DistroFamily::RedHat)
                code_bin_url = "https://go.microsoft.com/fwlink/?LinkID=760867";
            else {
                code_bin_url = "https://go.microsoft.com/fwlink/?LinkID=620884";
                code_bin_folder = "VSCode-linux-x64";
            }
        } else if (arch == "arm64") {
            if (family == DistroFamily::Debian)
                code_bin_url = "https://aka.ms/linux-arm64-deb";
            else if (family == DistroFamily::RedHat)
                code_bin_url = "https://aka.ms/linux-arm64-rpm";
            else {
                code_bin_url = "https://aka.ms/linux-arm64";
                code_bin_folder = "VSCode-linux-arm64";
            }
        } else if (arch == "armhf") {
            if (family == DistroFamily::Debian)
                code_bin_url = "https://aka.ms/linux-armhf-deb";
            else if (family == DistroFamily::RedHat)
                code_bin_url = "https://aka.ms/linux-armhf-rpm";
            else {
                code_bin_url = "https://aka.ms/linux-armhf";
                code_bin_folder = "VSCode-linux-armhf";
            }
        } else {
            Logger::error(_("devtools.error.vscode_arch_not_supported") + ": " + arch);
            return;
        }

        Logger::info(_("devtools.hint.download_url") + code_bin_url);

        // ── 公共 aria2c 标志：断点续传 + 实时进度 ──
        const char *aria2_flags = "--console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M";

        if (family == DistroFamily::Debian) {
            // Download and install .deb
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "aria2c " + std::string(aria2_flags) + " -o 'VSCODE.deb' '" + code_bin_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCODE.deb") || fs::file_size("/tmp/VSCODE.deb") == 0) {
                Logger::error(_("devtools.error.vscode_download_failed"));
                return;
            }
            Executor::passthrough(
                "cd /tmp && "
                "apt-cache show ./VSCODE.deb 2>/dev/null; "
                "dpkg -i ./VSCODE.deb || apt install -y ./VSCODE.deb; "
                "rm -vf VSCODE.deb"
            );
            Logger::ok(_("devtools.ok.vscode_install_done"));
        } else if (family == DistroFamily::RedHat) {
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "aria2c " + std::string(aria2_flags) + " -o 'VSCODE.rpm' '" + code_bin_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCODE.rpm") || fs::file_size("/tmp/VSCODE.rpm") == 0) {
                Logger::error(_("devtools.error.vscode_download_failed"));
                return;
            }
            Executor::passthrough("cd /tmp && yum install -y ./VSCODE.rpm; rm -vf VSCODE.rpm");
            Logger::ok(_("devtools.ok.vscode_install_done"));
        } else {
            // Generic tar.gz install
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "aria2c " + std::string(aria2_flags) + " -o 'VSCODE.tar.gz' '" + code_bin_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCODE.tar.gz") || fs::file_size("/tmp/VSCODE.tar.gz") == 0) {
                Logger::error(_("devtools.error.vscode_download_failed"));
                return;
            }
            Executor::passthrough(
                "cd /tmp && "
                "tar -zxvf VSCODE.tar.gz -C /usr/share && "
                "rm -rvf /usr/share/code 2>/dev/null; "
                "mv /usr/share/" + code_bin_folder + " /usr/share/code; "
                "rm -vf VSCODE.tar.gz"
            );

            // Create .electron marker
            Executor::shell("echo '" + code_bin_folder + "' > /usr/share/code/.electron");

            // Download share files (icons, desktop, mime, etc.)
            auto share_ret = Executor::passthrough(
                "cd /tmp && "
                "CODE_SHARE_FILE='.VSCODE_USR_SHARE.tar.xz'; "
                "aria2c " + std::string(aria2_flags) + " -o ${CODE_SHARE_FILE} "
                "https://gitee.com/ak2/vscode-share/raw/master/code.tar.xz || "
                "aria2c " + std::string(aria2_flags) + " -o ${CODE_SHARE_FILE} "
                "https://github.com/2moe/vscode-share/raw/master/code.tar.xz"
            );
            if (!share_ret.ok()) {
                Logger::warn(_("devtools.warn.vscode_share_failed"));
            } else {
                Executor::passthrough(
                    "cd /tmp && tar -Jxvf .VSCODE_USR_SHARE.tar.xz -C /; rm -vf .VSCODE_USR_SHARE.tar.xz"
                );
            }

            // Symlink
            Executor::shell(CommandBuilder("ln").add_flag("-sfv")
                .add_arg("/usr/share/code/bin/code").add_arg("/usr/bin").build_string());
            Logger::ok(_("devtools.ok.vscode_install_done"));
        }

        // Install code-no-sandbox.desktop if code exists
        if (fs::exists("/usr/share/code/code") || fs::exists("/usr/share/code/bin/code")) {
            std::string desktop_dir = apps_lnk_dir_.string();
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=Code No Sandbox\n"
                    "Comment=Code Editing. No sandbox. Redefined.\n"
                    "GenericName=Text Editor\n"
                    "Exec=/usr/share/code/code --no-sandbox --unity-launch %F\n"
                    "Icon=com.visualstudio.code\n"
                    "Type=Application\n"
                    "StartupNotify=false\n"
                    "StartupWMClass=Code\n"
                    "Categories=TextEditor;Development;IDE;\n"
                    "MimeType=text/plain;inode/directory;application/x-code-workspace;\n"
                    "Actions=new-empty-window;\n"
                    "Keywords=vscode;\n\n"
                    "X-Desktop-File-Install-Version=0.26\n\n"
                    "[Desktop Action new-empty-window]\n"
                    "Name=New Empty Window\n"
                    "Exec=/usr/share/code/code --no-sandbox --new-window %F\n"
                    "Icon=com.visualstudio.code\n";
            Executor::shell("cat > " + desktop_dir + "/code-no-sandbox.desktop <<'DESKTOPEOF'\n" +
                            desktop_content + "DESKTOPEOF\n" +
                            "chmod a+r " + desktop_dir + "/code-no-sandbox.desktop");
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // VS Code Server (code-server Web版)
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::run_vscode_server_menu() {
        std::string arch = cfg_.arch;

        // Check arch support
        if (arch != "arm64" && arch != "amd64" && arch != "armhf") {
            Logger::error(_("devtools.error.codeserver_no_arch"));
            Logger::info(_("devtools.hint.choose_other_version"));
            return;
        }

        if (!fs::exists("/usr/local/bin/code-server-data/code-server")) {
            // Not installed yet — install or configure
            std::string choice = cfg_.tui_bin +
                                 " --title \"您想要对这个小可爱做什么呢\""
                                 " --yes-button \"install安装\" --no-button \"Configure配置\""
                                 " --yesno \"检测到您尚未安装vscode-server\\n"
                                 "Visual Studio Code is a lightweight but powerful source code editor. "
                                 "It comes with built-in support for JavaScript, TypeScript and Node.js. "
                                 "♪(^∇^*) \" 16 50";
            auto result = Executor::passthrough(choice);
            if (result.exit_code == 0) {
                vscode_server_upgrade();
            } else {
                configure_vscode_server();
            }
        } else {
            // Already installed — check status
            auto pgrep_result = Executor::shell("pgrep node 2>/dev/null || true");
            bool running = pgrep_result.exit_code == 0;

            std::string status = running ? "检测到code-server进程正在运行" : "检测到code-server进程未运行";
            std::string btn = running ? "Restart重启" : "Start启动";

            std::string choice = cfg_.tui_bin +
                                 " --title \"你想要对这个小可爱做什么\""
                                 " --yes-button \"" + btn + "\" --no-button \"Configure配置\""
                                 " --yesno \"" + status + "\" 9 50";
            auto result = Executor::passthrough(choice);
            if (result.exit_code == 0) {
                vscode_server_restart();
            } else {
                configure_vscode_server();
            }
        }
    }

    void DeveloperTools::configure_vscode_server() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"CONFIGURE VSCODE_SERVER\""
                               " --menu \"您想要修改哪项配置？\" 0 50 0 "
                               "\"1\" \"upgrade code-server 更新/升级\" "
                               "\"2\" \"password 设定密码\" "
                               "\"3\" \"edit config manually 手动编辑配置\" "
                               "\"4\" \"stop 停止\" "
                               "\"5\" \"remove 卸载/移除\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") vscode_server_upgrade();
            else if (ch == "2") vscode_server_password();
            else if (ch == "3") Executor::passthrough("nano ~/.config/code-server/config.yaml");
            else if (ch == "4") {
                Logger::info(_("devtools.status.stopping_service"));
                Executor::shell("pkill node 2>/dev/null || true");
            } else if (ch == "5") vscode_server_remove();

            Logger::press_enter();
        }
    }

    void DeveloperTools::vscode_server_upgrade() {
        Logger::step(_("devtools.step.checking_version"));

        std::string local_ver = "NOT-INSTALLED";
        if (Executor::has("code-server")) {
            auto result = Executor::shell(
                "code-server --version 2>/dev/null | grep -v info | head -n 1 | awk '{print $1}'");
            local_ver = result.stdout_data;
            if (local_ver.empty()) local_ver = "unknown";
        }

        // Fetch latest version
        std::string latest_ver;
        auto ver_result = Executor::shell(
            "curl -sL https://gitee.com/mo2/vscode-server/raw/aarch64/version.txt 2>/dev/null | head -n 1"
        );
        latest_ver = ver_result.stdout_data;

        Logger::info("╔═══╦══════════╦═══════════════════╦════════════════════");
        Logger::info("║   ║          ║                   ║");
        Logger::info("║   ║ software ║    ✨" + _("devtools.table.latest_version") + "     ║   " + _("devtools.table.local_version") + " 🎪");
        Logger::info("║---║----------║-------------------║--------------------");
        Logger::info("║ 1 ║ vscode   ║ " + latest_ver + " ║ " + local_ver);
        Logger::info("║   ║ server   ║                   ║");
        Logger::info("");
        Logger::info(_("devtools.hint.start_codeserver"));

        if (!confirm("devtools.confirm_upgrade", "是否继续升级?"))
            return;

        std::string arch = cfg_.arch;
        std::string tmp_arch = (arch == "armhf") ? "armv7l" : arch;

        Executor::shell(CommandBuilder("chmod").add_arg("a+rx")
            .add_arg("/usr/local/bin/code-server-data/code-server")
            .add_raw("2>/dev/null || true").build_string());

        if (arch == "arm64") {
            Executor::shell(
                "cd /tmp && "
                "rm -rvf .VSCODE_SERVER_TEMP_FOLDER 2>/dev/null; "
                "git clone -b aarch64 --depth=1 https://gitee.com/mo2/vscode-server.git .VSCODE_SERVER_TEMP_FOLDER && "
                "cd .VSCODE_SERVER_TEMP_FOLDER && "
                "tar -PpJxvf code.tar.xz && "
                "cd .. && "
                "rm -rf /tmp/.VSCODE_SERVER_TEMP_FOLDER"
            );
        } else if (arch == "amd64" || arch == "armhf") {
            // 先抓取下载链接
            auto link_result = Executor::shell(
                "curl -sL https://api.github.com/repos/cdr/code-server/releases 2>/dev/null | "
                "grep '" + tmp_arch + "' | grep browser_download_url | grep linux | head -n 1 | "
                "awk -F '\"' '{print $4}'"
            );
            std::string server_url = link_result.stdout_data;
            while (!server_url.empty() && (server_url.back() == '\n' || server_url.back() == '\r'))
                server_url.pop_back();

            if (server_url.empty()) {
                Logger::error(_("devtools.error.codeserver_link_failed"));
                return;
            }

            Logger::info(_("devtools.hint.download_url") + server_url);
            Logger::step(_("devtools.step.downloading_codeserver"));

            // 用 passthrough 显示实时进度
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "mkdir -pv .VSCODE_SERVER_TEMP_FOLDER && "
                "cd .VSCODE_SERVER_TEMP_FOLDER && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M -o .VSCODE_SERVER.tar.gz '" + server_url + "'"
            );

            if (!dl_ret.ok()) {
                Logger::error(_("devtools.error.codeserver_download_failed"));
                return;
            }

            // 验证下载后文件存在且非空
            if (!fs::exists("/tmp/.VSCODE_SERVER_TEMP_FOLDER/.VSCODE_SERVER.tar.gz") ||
                fs::file_size("/tmp/.VSCODE_SERVER_TEMP_FOLDER/.VSCODE_SERVER.tar.gz") == 0) {
                Logger::error(_("devtools.error.codeserver_download_incomplete"));
                return;
            }

            // 解压并安装
            Executor::passthrough(
                "cd /tmp/.VSCODE_SERVER_TEMP_FOLDER && "
                "tar -zxvf .VSCODE_SERVER.tar.gz && "
                "VSCODE_FOLDER_NAME=$(ls -l ./ | grep '^d' | awk -F ' ' '$0=$NF') && "
                "rm -rvf /usr/local/bin/code-server-data /usr/local/bin/code-server && "
                "mv ${VSCODE_FOLDER_NAME} /usr/local/bin/code-server-data && "
                "ln -sf /usr/local/bin/code-server-data/bin/code-server /usr/local/bin/code-server"
            );
        }

        vscode_server_restart();
        vscode_server_password();

        Logger::info(_("devtools.hint.codeserver_first_install"));

        // Fix bind address
        Executor::shell(
            "if grep -q '127.0.0.1:8080' \"${HOME}/.config/code-server/config.yaml\" 2>/dev/null; then "
            "sed -i 's@bind-addr:.*@bind-addr: 0.0.0.0:18080@' \"${HOME}/.config/code-server/config.yaml\"; fi"
        );

        Logger::press_enter();
    }

    void DeveloperTools::vscode_server_restart() {
        Logger::info(_("devtools.status.starting_codeserver"));
        Logger::info(_("devtools.hint.codeserver_usage"));
        Executor::shell("/usr/local/bin/code-server-data/bin/code-server &");

        auto port_result = Executor::shell(
            "grep bind-addr ${HOME}/.config/code-server/config.yaml 2>/dev/null | cut -d ':' -f 3"
        );
        std::string port = port_result.stdout_data;
        if (port.empty()) port = "18080";

        Logger::info(_("devtools.status.codeserver_local_addr") + port);

        auto ip_result = Executor::shell("ip -4 -br -c a 2>/dev/null | tail -n 1 | cut -d '/' -f 1 | cut -d 'P' -f 2");
        std::string ip = ip_result.stdout_data;
        if (!ip.empty())
            Logger::info(_("devtools.status.lan_addr") + ip + ":" + port);

        Logger::info(_("devtools.hint.stop_node"));
    }

    void DeveloperTools::vscode_server_password() {
        std::string cmd = cfg_.tui_bin +
                          " --inputbox \"请设定访问密码\\nPlease enter the password.\" 12 50 --title \"PASSWORD\"";
        auto result = Executor::passthrough(cmd + " 2>/tmp/vscode_passwd_out");
        // whiptail inputbox is tricky with passthrough; use a simpler approach
        Logger::info(_("devtools.hint.manual_set_password"));
    }

    void DeveloperTools::vscode_server_remove() {
        Logger::info(_("devtools.status.stopping_codeserver"));
        Executor::shell("pkill node 2>/dev/null || true");

        Logger::info(_("devtools.hint.press_enter_remove"));
        if (!confirm("devtools.confirm_remove_vscode_server", "确认移除 VS Code Server?"))
            return;

        Executor::shell(CommandBuilder("rm").add_flag("-rvf")
            .add_arg("/usr/local/bin/code-server-data/")
            .add_arg("/usr/local/bin/code-server")
            .add_arg("/tmp/sed-vscode.tmp")
            .add_raw("2>/dev/null").build_string());
        Logger::ok(_("devtools.ok.remove_success"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // VS Codium: install_vscodium()
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_vscodium() {
        std::string arch = cfg_.arch;
        std::string codium_arch;

        if (arch == "arm64") codium_arch = "arm64";
        else if (arch == "armhf") codium_arch = "arm";
        else if (arch == "amd64") codium_arch = "x64";
        else {
            Logger::error(_("devtools.error.codium_arch_not_supported") + ": " + arch);
            return;
        }

        // Check existing installation
        if (fs::exists("/usr/bin/codium")) {
            Logger::info(_("devtools.detected.codium_pkg"));
            Logger::info(_("devtools.hint.codium_launch_pkg"));
        } else if (fs::exists("/opt/vscodium-data/codium")) {
            Logger::info(_("devtools.detected.codium_tar"));
            Logger::info(_("devtools.hint.codium_launch"));
        }

        // Try to run
        if (Executor::has("codium"))
            Executor::shell("codium --no-sandbox 2>/dev/null &");

        Logger::info(_("devtools.hint.download_codium"));
        if (!confirm("devtools.confirm_download", "是否下载最新版安装包?"))
            return;

        auto family = PackageManager::detect_distro_family();

        const char *aria2_flags = "--console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M";

        if (family == DistroFamily::Debian) {
            // 抓取下载链接
            auto link_result = Executor::shell(
                "LatestVSCodiumLink=$(curl -sL https://mirrors.bfsu.edu.cn/github-release/VSCodium/vscodium/LatestRelease/ 2>/dev/null | "
                "grep " + arch +
                " | grep -v '.sha256' | grep '\\.deb' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                "echo \"https://mirrors.bfsu.edu.cn/github-release/VSCodium/vscodium/LatestRelease/${LatestVSCodiumLink}\""
            );
            std::string codium_url = link_result.stdout_data;
            while (!codium_url.empty() && (codium_url.back() == '\n' || codium_url.back() == '\r'))
                codium_url.pop_back();

            if (codium_url.empty()) {
                Logger::error(_("devtools.error.codium_link_failed"));
                return;
            }

            Logger::info(_("devtools.hint.download_url") + codium_url);

            // 下载 (实时进度)
            auto dl_ret = Executor::passthrough(
                "cd /tmp && aria2c " + std::string(aria2_flags) + " -o 'VSCodium.deb' '" + codium_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCodium.deb") || fs::file_size("/tmp/VSCodium.deb") == 0) {
                Logger::error(_("devtools.error.codium_download_failed"));
                return;
            }

            // 安装
            Executor::passthrough(
                "cd /tmp && apt-cache show ./VSCodium.deb 2>/dev/null; "
                "dpkg -i ./VSCodium.deb; rm -vf VSCodium.deb"
            );
            Logger::ok(_("devtools.ok.codium_install_done_pkg"));
        } else {
            // 抓取下载链接
            auto link_result = Executor::shell(
                "LatestVSCodiumLink=$(curl -sL https://mirrors.bfsu.edu.cn/github-release/VSCodium/vscodium/LatestRelease/ 2>/dev/null | "
                "grep " + codium_arch +
                " | grep -v '.sha256' | grep '.tar' | tail -n 1 | cut -d '=' -f 3 | cut -d '\"' -f 2) && "
                "echo \"https://mirrors.bfsu.edu.cn/github-release/VSCodium/vscodium/LatestRelease/${LatestVSCodiumLink}\""
            );
            std::string codium_url = link_result.stdout_data;
            while (!codium_url.empty() && (codium_url.back() == '\n' || codium_url.back() == '\r'))
                codium_url.pop_back();

            if (codium_url.empty()) {
                Logger::error(_("devtools.error.codium_link_failed"));
                return;
            }

            Logger::info(_("devtools.hint.download_url") + codium_url);

            // 下载 (实时进度)
            auto dl_ret = Executor::passthrough(
                "cd /tmp && aria2c " + std::string(aria2_flags) + " -o 'VSCodium.tar.gz' '" + codium_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCodium.tar.gz") || fs::file_size("/tmp/VSCodium.tar.gz") == 0) {
                Logger::error(_("devtools.error.codium_download_failed"));
                return;
            }

            // 解压
            Executor::passthrough(
                "mkdir -pv /opt/vscodium-data && "
                "cd /tmp && tar -zxvf VSCodium.tar.gz -C /opt/vscodium-data && "
                "rm -vf VSCodium.tar.gz"
            );

            // Create launcher script /usr/local/bin/codium
            std::string codium_script =
                    "#!/usr/bin/env bash\n"
                    "TMOE_BIN='/opt/vscodium-data/bin/codium'\n"
                    "case \"$(id -u)\" in\n"
                    "0) ${TMOE_BIN} --no-sandbox --user-data-dir=${HOME}/.codium \"$@\" ;;\n"
                    "*)\n"
                    "    ${TMOE_BIN} \"$@\"\n"
                    "    case \"$?\" in\n"
                    "    0) ;;\n"
                    "    *) ${TMOE_BIN} --no-sandbox --user-data-dir=${HOME}/.codium \"$@\" ;;\n"
                    "    esac\n"
                    "    ;;\n"
                    "esac\n";
            Executor::shell(
                "cat > /usr/local/bin/codium <<'CODIUMEOF'\n" + codium_script +
                "CODIUMEOF\nchmod a+rx /usr/local/bin/codium");

            // Create desktop file
            std::string lnk_dir = apps_lnk_dir_.string();
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=VSCodium\n"
                    "Comment=Code Editing. Redefined.\n"
                    "GenericName=Text Editor\n"
                    "Exec=/usr/local/bin/codium --unity-launch %F\n"
                    "Icon=/usr/share/icons/vscodium.png\n"
                    "Type=Application\n"
                    "StartupNotify=false\n"
                    "StartupWMClass=VSCodium\n"
                    "Categories=Utility;TextEditor;Development;IDE;\n"
                    "MimeType=text/plain;inode/directory;\n"
                    "Actions=new-empty-window;\n"
                    "Keywords=vscode;\n\n"
                    "X-Desktop-File-Install-Version=0.26\n\n"
                    "[Desktop Action new-empty-window]\n"
                    "Name=New Empty Window\n"
                    "Exec=/usr/local/bin/codium --new-window %F\n"
                    "Icon=vscodium\n";
            Executor::shell("cat > " + lnk_dir + "/codium.desktop <<'CODDESKEOF'\n" + desktop_content + "CODDESKEOF");

            // Download icon
            if (!fs::exists("/usr/share/icons/vscodium.png")) {
                Executor::passthrough(
                    "aria2c --console-log-level=warn --no-conf --continue=true "
                    "-d '/usr/share/icons' -o 'vscodium.png' "
                    "'https://gitee.com/ak2/icons/raw/master/vscodium.png' 2>/dev/null || true"
                );
            }

            Logger::ok(_("devtools.ok.codium_install_done"));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // 修复 tightvnc 下 vscode 无法启动
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::fix_tightvnc_vscode() {
        Logger::info(_("devtools.hint.deb_only"));
        Logger::info(_("devtools.hint.manual_fix_cmd"));
        Logger::info("env LD_LIBRARY_PATH=${TMOE_LINUX_DIR}/lib codium --user-data-dir=${HOME}/.codium");
        Logger::info("env LD_LIBRARY_PATH=${TMOE_LINUX_DIR}/lib code --user-data-dir=${HOME}/.vscode");

        auto family = PackageManager::detect_distro_family();
        if (family != DistroFamily::Debian) {
            Logger::warn(_("devtools.warn.non_deb_skip"));
            return;
        }

        // Find GNU libxcb
        std::string gnu_libxcb;
        auto find_result = Executor::shell(
            "dpkg -L libx11-xcb1 2>/dev/null | grep 'libxcb.so' | head -n 1"
        );
        gnu_libxcb = find_result.stdout_data;
        if (gnu_libxcb.empty()) {
            // Try alternative
            auto find2 = Executor::shell("find /usr/lib -name 'libxcb.so*' 2>/dev/null | head -n 1");
            gnu_libxcb = find2.stdout_data;
            if (gnu_libxcb.empty()) {
                Logger::error(_("devtools.error.libxcb_not_found"));
                return;
            }
        }

        // Remove trailing newlines
        while (!gnu_libxcb.empty() && (gnu_libxcb.back() == '\n' || gnu_libxcb.back() == '\r'))
            gnu_libxcb.pop_back();

        std::string tmoe_linux_dir = "/usr/local/etc/tmoe-linux";

        if (!fs::exists(tmoe_linux_dir + "/lib/libxcb.so.1")) {
            Executor::shell(
                "mkdir -pv " + tmoe_linux_dir + "/lib && "
                "cp " + gnu_libxcb + " " + tmoe_linux_dir + "/lib/libxcb.so.1 && "
                "sed -i 's@BIG-REQUESTS@_IG-REQUESTS@' " + tmoe_linux_dir + "/lib/libxcb.so.1"
            );
        }

        if (fs::exists(tmoe_linux_dir + "/lib/libxcb.so.1")) {
            std::string lnk_dir = apps_lnk_dir_.string();
            Executor::shell(
                "sed -i \"s@Exec=/usr/share/codium/codium@Exec=env LD_LIBRARY_PATH=" + tmoe_linux_dir +
                "/lib /usr/share/codium/codium --no-sandbox@g\" " + lnk_dir + "/codium.desktop 2>/dev/null || true; "
                "sed -i \"s@Exec=/usr/share/code/code@Exec=env LD_LIBRARY_PATH=" + tmoe_linux_dir +
                "/lib /usr/share/code/code --no-sandbox@g\" " + lnk_dir + "/code.desktop 2>/dev/null || true"
            );
            Logger::ok(_("devtools.ok.fix_done"));
        } else {
            Logger::error(_("devtools.error.fix_failed"));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Android Studio 准备
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::prep_android_studio() {
        grep_name_ = "android-studio";
        lnk_file_ = "android-studio.desktop";
        bin_file_ = "/usr/bin/android-studio";
        app_opt_dir_ = "/opt/android-studio";
        dev_menu_type_ = 2;
    }

    // ═══════════════════════════════════════════════════════════════════
    // IDEA 旗舰版 vs 社区版
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::choose_idea_edition() {
        std::string choice = cfg_.tui_bin +
                             " --title \"ultimate_edition_or_community_edition\""
                             " --yes-button 'ultimate' --no-button 'community'"
                             " --yesno \"Do you want to choose ultimate edition or community edition?\\n"
                             "ultimate为付费旗舰版本,community为免费社区版\" 0 0";
        auto result = Executor::passthrough(choice);

        if (result.exit_code == 0) {
            // Ultimate
            grep_name_ = "intellij-idea-ultimate-edition";
            lnk_file_ = "intellij-idea-ultimate-edition.desktop";
            bin_file_ = "/opt/intellij-idea-ultimate-edition/bin/idea.sh";
            icon_name_ = "intellij-idea-ultimate-edition.png";
            app_opt_dir_ = "/opt/intellij-idea-ultimate-edition";
            community_edition_ = false;
        } else {
            // Community
            community_edition_ = true;
            grep_name_ = "intellij-idea-community-edition";
            lnk_file_ = "intellij-idea-community-edition.desktop";
            bin_file_ = "/opt/intellij-idea-community-edition/bin/idea.sh";
            icon_name_ = "intellij-idea-community-edition.png";
            app_opt_dir_ = "/opt/intellij-idea-community-edition";
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // dev_menu_01 — JetBrains IDE 二级菜单 (install/delete/remove)
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::run_ide_submenu_01() {
        if (grep_name_.empty()) return;
        dev_menu_type_ = 1;

        while (true) {
            check_download_path();

            std::string title = grep_name_;
            std::string menu = cfg_.tui_bin + " --title \"" + title + "\""
                               " --menu \"您想要对 " + grep_name_ + " 小可爱做什么？\" 0 50 0 "
                               "\"1\" \"install/upgrade (安装/升级)\" "
                               "\"2\" \"del pkg (删除安装包)\" "
                               "\"3\" \"remove (卸载 " + grep_name_ + ")\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") install_ide_01();
            else if (ch == "2") delete_ide_pkg();
            else if (ch == "3") remove_ide_01();

            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // dev_menu_02 — Android Studio 二级菜单
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::run_ide_submenu_02() {
        if (grep_name_.empty()) return;
        dev_menu_type_ = 2;

        while (true) {
            check_download_path();
            // show_ide_version_table("", check_local_opt_version());

            std::string title = grep_name_;
            std::string menu = cfg_.tui_bin + " --title \"" + title + "\""
                               " --menu \"您想要对 " + grep_name_ + " 小可爱做什么？\" 0 50 0 "
                               "\"1\" \"install/upgrade (安装/升级)\" "
                               "\"2\" \"del pkg (删除安装包)\" "
                               "\"3\" \"remove (卸载 " + grep_name_ + ")\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") install_ide_02();
            else if (ch == "2") delete_ide_pkg_02();
            else if (ch == "3") remove_ide_02();

            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // install_ide_01 — JetBrains IDE 下载安装 (archlinuxcn tar.zst)
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_ide_01() {
        bool is_jetbrains = !jetbrains_product_code().empty();

        if (is_jetbrains) {
            // ── 纯净的 JetBrains 官方直链安装逻辑 ──
            icon_file_ = app_opt_dir_ + "/bin/" + icon_name_;
            Logger::step(_("devtools.step.checking_version"));

            std::string dl_url, dl_version;
            if (!fetch_jetbrains_link(dl_url, dl_version)) {
                if (grep_name_.find("intellij-idea") != std::string::npos) tip_manual_install(
                    "https://www.jetbrains.com/idea/download/#section=linux");
                else if (grep_name_.find("pycharm") != std::string::npos) tip_manual_install(
                    "https://www.jetbrains.com/pycharm/download/#section=linux");
                else if (grep_name_.find("webstorm") != std::string::npos) tip_manual_install(
                    "https://www.jetbrains.com/webstorm/download/#section=linux");
                else if (grep_name_.find("clion") != std::string::npos) tip_manual_install(
                    "https://www.jetbrains.com/clion/download/#section=linux");
                else if (grep_name_.find("goland") != std::string::npos) tip_manual_install(
                    "https://www.jetbrains.com/goland/download/#section=linux");
                return;
            }

            show_ide_version_table(dl_version, check_local_opt_version());

            if (!confirm("devtools.confirm_upgrade_ide", "是否安装/升级?"))
                return;

            // 提取直链文件名
            std::string filename = Executor::shell(CommandBuilder("basename").add_arg(dl_url).build_string()).stdout_data;
            while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r')) filename.pop_back();

            if (!download_and_extract_jetbrains(dl_url, filename)) {
                Logger::error(_("devtools.error.jetbrains_extract_failed"));
                return;
            }

            // 保存版本信息
            Executor::shell(
                "echo '" + dl_version + "' > " + download_path_.string() + "/" + grep_name_ +
                "-version.txt 2>/dev/null");

            install_java_if_needed();
            Logger::ok(grep_name_ + " " + dl_version + _("devtools.ok.jetbrains_install_done"));
        } else if (grep_name_ == "github-desktop-bin") {
            // ── 转向 GitHub Desktop 官方推荐的 Linux 原生包 ──
            install_github_desktop();
        }
    }

    void DeveloperTools::install_github_desktop() {
        if (cfg_.arch != "amd64") {
            Logger::error(_("devtools.error.github_amd64_only"));
            Logger::info(_("devtools.hint.official_repo") + "https://github.com/shiftkey/desktop");
            return;
        }

        Logger::step(_("devtools.step.fetch_github_api"));

        // 采用企业级加固的 curl 抓取 shiftkey 的最新 Release
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        // 安全策略：将 JSON 落地为临时文件，避免 Shell 单引号注入冲突
        std::string tmp_json = "/tmp/gh_desktop_release.json";
        Executor::shell(
            "curl " + std::string(curl_opts) + " 'https://api.github.com/repos/shiftkey/desktop/releases/latest' -o " +
            tmp_json);

        // 检查是否下载成功且包含有效的 API 字段
        auto check_ret = Executor::shell("grep '\"assets\"' " + tmp_json + " > /dev/null 2>&1");
        if (check_ret.exit_code != 0) {
            Logger::error(_("devtools.error.github_api_rate_limit"));
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(tmp_json)
                .add_raw("2>/dev/null").build_string());
            return;
        }

        // 从文件中提取版本号
        std::string latest_ver = Executor::shell("jq -r '.tag_name' " + tmp_json + " | sed 's/^release-//'").
                stdout_data;
        while (!latest_ver.empty() && (latest_ver.back() == '\n' || latest_ver.back() == '\r')) latest_ver.pop_back();

        show_ide_version_table(latest_ver, check_local_opt_version());

        if (!confirm("devtools.confirm_upgrade_ide", "是否安装/升级 GitHub Desktop?")) {
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(tmp_json)
                .add_raw("2>/dev/null").build_string());
            return;
        }

        // 智能探测后缀：Debian用deb，RedHat用rpm，兜底用AppImage
        auto family = PackageManager::detect_distro_family();
        std::string ext = ".AppImage";
        if (family == DistroFamily::Debian) ext = ".deb";
        else if (family == DistroFamily::RedHat) ext = ".rpm";

        // 提取下载直链 (读取落地文件)
        std::string jq_cmd = "jq -r '.assets[] | select(.name | endswith(\"" + ext + "\")) | .browser_download_url' " +
                             tmp_json + " | head -n 1";
        std::string dl_url = Executor::shell(jq_cmd).stdout_data;
        while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();

        // 如果该版本没提供对应的原生包，降级到通用 AppImage
        if (dl_url.empty() || dl_url == "null") {
            Logger::warn(_("devtools.warn.fallback_appimage") + ext + "，自动降级为通用 AppImage...");
            ext = ".AppImage";
            jq_cmd = "jq -r '.assets[] | select(.name | endswith(\".AppImage\")) | .browser_download_url' " + tmp_json +
                     " | head -n 1";
            dl_url = Executor::shell(jq_cmd).stdout_data;
            while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();
        }

        // 用完即焚，清理临时文件
        Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(tmp_json)
            .add_raw("2>/dev/null").build_string());

        if (dl_url.empty() || dl_url == "null") {
            Logger::error(_("devtools.error.github_no_link"));
            return;
        }

        std::string filename = Executor::shell(CommandBuilder("basename").add_arg(dl_url).build_string()).stdout_data;
        while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r')) filename.pop_back();

        check_download_path();
        std::string dest = download_path_.string() + "/" + filename;

        if (!aria2_download(dl_url, dest, true)) {
            Logger::error(_("devtools.error.ide_download_failed"));
            return;
        }

        Logger::step(_("devtools.step.native_install"));

        // 依据包类型执行不同策略
        if (ext == ".deb") {
            Executor::passthrough("sudo dpkg -i '" + dest + "' || sudo apt-get install -f -y");
        } else if (ext == ".rpm") {
            Executor::passthrough("sudo rpm -i '" + dest + "' || sudo yum localinstall -y '" + dest + "'");
        } else {
            // AppImage 通用策略
            Executor::shell(CommandBuilder("mkdir").add_flag("-p")
                .add_arg("/opt/github-desktop").build_string());
            Executor::shell(CommandBuilder("cp").add_flag("-f").add_arg(dest)
                .add_arg("/opt/github-desktop/GitHubDesktop.AppImage").build_string());
            Executor::shell(CommandBuilder("chmod").add_arg("+x")
                .add_arg("/opt/github-desktop/GitHubDesktop.AppImage").build_string());

            // 创建桌面图标
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=GitHub Desktop\n"
                    "Comment=Simple collaboration from your desktop\n"
                    "Exec=/opt/github-desktop/GitHubDesktop.AppImage --no-sandbox %U\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Icon=github-desktop\n"
                    "Categories=Development;\n";
            Executor::shell(
                "cat > " + apps_lnk_dir_.string() + "/github-desktop.desktop <<'EOF'\n" + desktop_content + "EOF\n");
            // 获取图标
            Executor::shell(
                "curl -sL https://raw.githubusercontent.com/shiftkey/desktop/development/app/static/logos/256x256.png -o /usr/share/pixmaps/github-desktop.png 2>/dev/null || true");
        }

        // 记录版本，供菜单比对
        Executor::shell(
            "echo '" + latest_ver + "' > " + download_path_.string() + "/" + grep_name_ + "-version.txt 2>/dev/null");
        Logger::ok(_("devtools.ok.github_install_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // install_ide_02 — Android Studio 下载安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_ide_02() {
        icon_file_ = "/opt/android-studio/bin/studio.png";

        Logger::step(_("devtools.step.checking_version"));
        tip_manual_install("https://developer.android.com/studio");

        // ── 抓取 Android Studio 下载链接 ──
        // 使用精确正则匹配，避免 grep href 误匹配
        auto link_result = Executor::shell(
            "curl -sL 'https://developer.android.google.cn/studio/' 2>/dev/null | "
            "grep -oE 'https://[^\"]*android-studio[^\"]*linux\\.tar\\.gz' | head -n 1"
        );
        std::string latest_link = link_result.stdout_data;
        // 去除尾部换行符
        while (!latest_link.empty() && (latest_link.back() == '\n' || latest_link.back() == '\r'))
            latest_link.pop_back();

        // 抓取失败时直接阻断，防止后续 aria2c 报错
        if (latest_link.empty()) {
            Logger::error(_("devtools.error.as_link_failed"));
            return;
        }

        // 从链接中提取文件名和版本号
        std::string download_file_name = Executor::shell(
            CommandBuilder("basename").add_arg(latest_link).build_string()
        ).stdout_data;
        while (!download_file_name.empty() && (download_file_name.back() == '\n' || download_file_name.back() == '\r'))
            download_file_name.pop_back();

        // 兼容 android-studio-ide- 和 android-studio- 两种命名规范
        std::string latest_ver = Executor::shell(
            "echo '" + download_file_name + "' | sed -E 's/android-studio(-ide)?-(.*)-linux\\.tar\\.gz/\\2/'"
        ).stdout_data;
        while (!latest_ver.empty() && (latest_ver.back() == '\n' || latest_ver.back() == '\r'))
            latest_ver.pop_back();

        show_ide_version_table(latest_ver, check_local_opt_version());

        if (!confirm("devtools.confirm_upgrade_ide", "是否安装/升级?"))
            return;

        // 传参给下载函数，避免重复抓取
        if (!download_android_studio(latest_link, download_file_name)) {
            Logger::error(_("devtools.error.as_download_failed"));
            return;
        }

        if (!extract_android_studio()) {
            Logger::error(_("devtools.error.as_extract_failed"));
            return;
        }

        create_android_studio_desktop();
        install_java_if_needed();

        // 保存版本信息
        Executor::shell("echo '" + latest_ver + "' > " + download_path_.string() +
                        "/" + grep_name_ + "-version.txt 2>/dev/null");

        Logger::ok(_("devtools.ok.as_install_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // delete_ide_pkg — 删除下载的 tar.zst 安装包
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::delete_ide_pkg() {
        Logger::info(_("devtools.status.clean_cache_dir") + download_path_.string());

        // 动态推断当前 IDE 的包名关键词
        std::string pattern = "*";
        if (grep_name_.find("idea") != std::string::npos) pattern = "*idea*";
        else if (grep_name_.find("pycharm") != std::string::npos) pattern = "*pycharm*";
        else if (grep_name_ == "webstorm") pattern = "*WebStorm*";
        else if (grep_name_ == "clion-1") pattern = "*CLion*";
        else if (grep_name_ == "goland") pattern = "*GoLand*";
        else if (grep_name_ == "github-desktop-bin") pattern = "*GitHubDesktop*";

        // 搜索可能存在的原生包和源码包
        auto ls_result = Executor::shell(
            "cd " + download_path_.string() + " && ls -lh " + pattern + ".tar.gz " + pattern + ".deb " + pattern +
            ".rpm " + pattern + ".AppImage 2>/dev/null");

        if (ls_result.stdout_data.empty()) {
            Logger::warn(_("devtools.warn.no_pkg_cache"));
            return;
        }

        Logger::info(_("devtools.status.found_cache_files"));
        Logger::info(ls_result.stdout_data);

        if (confirm("devtools.confirm_delete_pkg", "确认删除这些安装包吗?")) {
            Executor::shell(
                "cd " + download_path_.string() + " && rm -v " + pattern + ".tar.gz " + pattern + ".deb " + pattern +
                ".rpm " + pattern + ".AppImage 2>/dev/null || true");
            Logger::ok(_("devtools.ok.cleanup_done"));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // delete_ide_pkg_02 — 删除下载的 tar.gz 安装包
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::delete_ide_pkg_02() {
        auto ls_result = Executor::shell(
            "cd " + download_path_.string() + " && "
            "ls -t " + grep_name_ + "*.tar.gz 2>/dev/null | head -n 1"
        );
        std::string local_pkg = ls_result.stdout_data;
        while (!local_pkg.empty() && (local_pkg.back() == '\n' || local_pkg.back() == '\r'))
            local_pkg.pop_back();

        if (local_pkg.empty()) {
            Logger::warn(_("devtools.warn.pkg_not_exist"));
        } else {
            Logger::info("rm -v " + download_path_.string() + "/" + local_pkg);
            auto size_result = Executor::shell("ls -lh " + download_path_.string() + "/" + local_pkg + " 2>/dev/null");
            Logger::info(size_result.stdout_data);

            if (confirm("devtools.confirm_delete_pkg", "Do you want to delete it?")) {
                Executor::shell(CommandBuilder("rm").add_flag("-v")
                    .add_arg(download_path_.string() + "/" + local_pkg).build_string());
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // remove_ide_01 — 卸载 JetBrains IDE / GitHub Desktop
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::remove_ide_01() {
        std::string lnk_dir = apps_lnk_dir_.string();
        std::string dl_path = download_path_.string();
        std::string version_file = dl_path + "/" + grep_name_ + "-version.txt";
        std::string manifest_file = dl_path + "/" + grep_name_ + ".manifest";

        bool is_jetbrains = !jetbrains_product_code().empty();

        std::string cli_link;
        if (is_jetbrains) {
            if (grep_name_.find("intellij-idea") != std::string::npos) cli_link = "/usr/local/bin/idea";
            else if (grep_name_.find("pycharm") != std::string::npos) cli_link = "/usr/local/bin/pycharm";
            else if (grep_name_ == "webstorm") cli_link = "/usr/local/bin/webstorm";
            else if (grep_name_ == "clion-1") cli_link = "/usr/local/bin/clion";
            else if (grep_name_ == "goland") cli_link = "/usr/local/bin/goland";
        }

        Logger::warn("═══════════════════════════════════════════");
        Logger::warn(_("devtools.warn.uninstalling") + grep_name_);
        Logger::warn("═══════════════════════════════════════════");
        Logger::warn(_("devtools.warn.install_dir") + app_opt_dir_);
        Logger::warn(_("devtools.warn.desktop_icon") + lnk_dir + "/" + lnk_file_);
        if (!cli_link.empty())
            Logger::warn(_("devtools.warn.cli_link") + cli_link);
        Logger::warn(_("devtools.warn.version_record") + version_file);
        if (fs::exists(manifest_file))
            Logger::warn(_("devtools.warn.manifest_file") + manifest_file + _("devtools.warn.manifest_deep_clean"));

        if (!confirm("devtools.confirm_remove_ide", "确认卸载 " + grep_name_ + " ?"))
            return;

        // 基于 Manifest 的安全深度清理
        if (fs::exists(manifest_file)) {
            Logger::step(_("devtools.step.manifest_clean"));
            // 倒序读取文件（先删深层文件，再处理目录），仅删除文件和空目录
            std::string clean_cmd =
                    "tac '" + manifest_file + "' | while read -r item; do "
                    "  target=\"/${item}\"; "
                    "  if [ -f \"$target\" ] || [ -L \"$target\" ]; then "
                    "    rm -f \"$target\"; "
                    "  elif [ -d \"$target\" ]; then "
                    "    rmdir \"$target\" 2>/dev/null || true; "
                    "  fi; "
                    "done";
            Executor::shell(clean_cmd);
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(manifest_file)
                .add_raw("2>/dev/null").build_string());
        }

        if (is_jetbrains) {
            Executor::shell(CommandBuilder("rm").add_flag("-rf").add_arg(app_opt_dir_)
                .add_raw("2>/dev/null").build_string());
            Executor::shell(CommandBuilder("rm").add_flag("-f")
                .add_arg(lnk_dir + "/" + lnk_file_)
                .add_raw("2>/dev/null").build_string());
            if (!cli_link.empty()) Executor::shell(CommandBuilder("rm").add_flag("-f")
                .add_arg(cli_link).add_raw("2>/dev/null").build_string());
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(version_file)
                .add_raw("2>/dev/null").build_string());

            if (grep_name_.find("intellij-idea-community") != std::string::npos) {
                Executor::shell(CommandBuilder("rm").add_flag("-rf")
                    .add_arg("/usr/share/licenses/idea")
                    .add_arg("/usr/share/icons/hicolor/scalable/apps/idea.svg")
                    .add_raw("2>/dev/null").build_string());
            }
            if (grep_name_.find("pycharm-community") != std::string::npos) {
                Executor::shell(CommandBuilder("rm").add_flag("-rf")
                    .add_arg("/usr/share/licenses/pycharm")
                    .add_arg("/usr/share/icons/hicolor/scalable/apps/pycharm.svg")
                    .add_raw("2>/dev/null").build_string());
            }
        } else if (grep_name_ == "github-desktop-bin") {
            Logger::step(_("devtools.step.removing_github_desktop"));
            // 如果是通过原生包安装的，需要用对应的包管理器踢掉
            PackageManager::remove("github-desktop", PackageManager::detect_distro_family());

            // 清理残余的 AppImage 配置和版本号文件
            Executor::shell(CommandBuilder("rm").add_flag("-rvf")
                .add_arg("/opt/github-desktop")
                .add_arg("/usr/share/pixmaps/github-desktop.png")
                .add_arg(apps_lnk_dir_.string() + "/github-desktop.desktop")
                .add_arg(download_path_.string() + "/" + grep_name_ + "-version.txt")
                .add_raw("2>/dev/null").build_string());
        } else {
            Executor::shell(CommandBuilder("rm").add_flag("-rvf")
                .add_arg(app_opt_dir_)
                .add_arg(lnk_dir + "/" + lnk_file_)
                .add_arg("/usr/share/pixmaps/" + icon_name_)
                .add_arg(bin_file_)
                .add_arg(version_file)
                .add_raw("2>/dev/null").build_string());
        }

        Logger::ok(grep_name_ + _("devtools.ok.uninstall_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // remove_ide_02 — 卸载 Android Studio
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::remove_ide_02() {
        std::string lnk_dir = apps_lnk_dir_.string();
        std::string dl_path = download_path_.string();
        std::string version_file = dl_path + "/" + grep_name_ + "-version.txt";

        Logger::warn("═══════════════════════════════════════════");
        Logger::warn(_("devtools.warn.uninstalling") + grep_name_);
        Logger::warn("═══════════════════════════════════════════");
        Logger::warn(_("devtools.warn.install_dir") + app_opt_dir_);
        Logger::warn(_("devtools.warn.desktop_icon") + lnk_dir + "/" + lnk_file_);
        Logger::warn(_("devtools.warn.version_record") + version_file);

        if (!confirm("devtools.confirm_remove_ide", "确认卸载 " + grep_name_ + " ?"))
            return;

        // 主目录 + 桌面图标 + 版本记录
        Executor::shell(CommandBuilder("rm").add_flag("-rf").add_arg(app_opt_dir_)
            .add_raw("2>/dev/null").build_string());
        Executor::shell(CommandBuilder("rm").add_flag("-f")
            .add_arg(lnk_dir + "/" + lnk_file_)
            .add_raw("2>/dev/null").build_string());
        Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(version_file)
            .add_raw("2>/dev/null").build_string());

        Logger::ok(grep_name_ + _("devtools.ok.uninstalled"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // GNU Emacs — 快速安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_emacs() {
        Logger::step(_("devtools.step.installing_emacs"));
        auto family = PackageManager::detect_distro_family();
        PackageManager::install("emacs", family);
        Logger::ok(_("devtools.ok.emacs_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // Code::Blocks — 快速安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_code_blocks() {
        Logger::step(_("devtools.step.installing_codeblocks"));
        auto family = PackageManager::detect_distro_family();
        PackageManager::install("codeblocks", family);
        Logger::ok(_("devtools.ok.codeblocks_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // Sublime Text — 添加官方源 + 安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_sublime_text() {
        std::string arch = cfg_.arch;
        if (arch != "amd64" && arch != "i386") {
            Logger::error(_("devtools.error.sublime_arch_only"));
            return;
        }

        auto family = PackageManager::detect_distro_family();

        switch (family) {
            case DistroFamily::Debian: {
                Logger::step(_("devtools.step.adding_sublime_source"));
                Executor::passthrough(
                    "curl -L https://download.sublimetext.com/sublimehq-pub.gpg 2>/dev/null | "
                    "gpg --dearmor > /tmp/sublimehq-pub.gpg && "
                    "sudo install -o root -g root -m 644 /tmp/sublimehq-pub.gpg "
                    "/usr/share/keyrings/sublimehq-pub-archive-keyring.gpg && "
                    "echo 'deb [signed-by=/usr/share/keyrings/sublimehq-pub-archive-keyring.gpg] "
                    "https://download.sublimetext.com/ apt/stable/' | "
                    "sudo tee /etc/apt/sources.list.d/sublime-text.list"
                );
                PackageManager::update(family);
                PackageManager::install("sublime-text", family);
                break;
            }
            case DistroFamily::Arch: {
                Executor::shell(
                    "cd /tmp && "
                    "curl -O https://download.sublimetext.com/sublimehq-pub.gpg 2>/dev/null && "
                    "pacman-key --add sublimehq-pub.gpg 2>/dev/null && "
                    "pacman-key --lsign-key 8A8F901A 2>/dev/null && "
                    "rm sublimehq-pub.gpg"
                );
                Executor::shell(
                    "if ! grep -q 'sublimetext' /etc/pacman.conf 2>/dev/null; then "
                    "echo -e '\\n[sublime-text]\\nServer = https://download.sublimetext.com/arch/stable/x86_64' | "
                    "sudo tee -a /etc/pacman.conf; fi"
                );
                PackageManager::update(family);
                PackageManager::install("sublime-text", family);
                break;
            }
            case DistroFamily::RedHat:
            case DistroFamily::Suse: {
                Executor::shell(
                    "rpm -v --import https://download.sublimetext.com/sublimehq-rpm-pub.gpg 2>/dev/null || true");

                if (Executor::has("dnf")) {
                    Executor::shell(
                        "dnf config-manager --add-repo https://download.sublimetext.com/rpm/stable/x86_64/sublime-text.repo 2>/dev/null || true");
                } else if (Executor::has("yum")) {
                    Executor::shell(
                        "yum-config-manager --add-repo https://download.sublimetext.com/rpm/stable/x86_64/sublime-text.repo 2>/dev/null || true");
                } else if (Executor::has("zypper")) {
                    Executor::shell(
                        "zypper addrepo -g -f https://download.sublimetext.com/rpm/stable/x86_64/sublime-text.repo 2>/dev/null || true");
                }
                PackageManager::install("sublime-text", family);
                break;
            }
            default:
                PackageManager::install("sublime-text", family);
                break;
        }

        Logger::ok(_("devtools.ok.sublime_done"));
    }

    // ═══════════════════════════════════════════════════════════════════
    // 辅助方法实现
    // ═══════════════════════════════════════════════════════════════════

    void DeveloperTools::check_download_path() {
        if (!fs::exists(download_path_)) {
            fs::create_directories(download_path_);
        }
    }

    std::string DeveloperTools::fetch_latest_archlinuxcn_version() {
        // 统一配置 curl 参数：强制 IPv4、重试 3 次、伪装请求头
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 45 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        if (community_edition_) {
            // ── 社区版 → Arch Linux community 仓库 ──
            Logger::info(_("devtools.status.checking_arch_community") + grep_name_ + " ...");
            auto result = Executor::shell(
                "curl " + std::string(curl_opts) +
                " 'https://archlinux.org/packages/community/x86_64/" + grep_name_ + "/' 2>/dev/null | "
                "grep -oP '(?<=<meta itemprop=\"softwareVersion\" content=\")[^\"]+' | head -n 1"
            );
            std::string ver = result.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();

            if (!ver.empty()) {
                Logger::info(_("devtools.status.latest_version") + ver + " (Arch community)");
                return ver;
            }

            // 回退：直接扫 community 镜像目录列表
            Logger::info(_("devtools.status.parsing_failed_try_mirror"));
            auto fallback = Executor::shell(
                "curl " + std::string(curl_opts) +
                " 'https://mirrors.kernel.org/archlinux/community/os/x86_64/' 2>/dev/null | "
                "grep -oP '" + grep_name_ + "-\\K[0-9][0-9._-]+(?=-x86_64\\.pkg\\.tar\\.zst)' | "
                "sort -V | tail -n 1"
            );
            ver = fallback.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();
            if (!ver.empty()) Logger::info(_("devtools.status.latest_version") + ver + " (Arch community mirror)");
            return ver;
        } else {
            // ── 旗舰版/AUR → archlinuxcn 仓库 ──
            Logger::info(_("devtools.status.checking_archlinuxcn") + grep_name_ + " ...");
            auto result = Executor::shell(
                "curl " + std::string(curl_opts) +
                " 'https://mirrors.bfsu.edu.cn/archlinuxcn/x86_64/' 2>/dev/null | "
                "grep '\\.pkg\\.tar\\.zst' | grep -Ev '\\.xz\\.sig|\\.zst\\.sig' | "
                "grep -v '\\-jre\\-' | grep '" + grep_name_ + "' | tail -n 1 | "
                "sed 's@.*" + grep_name_ + "-@@' | sed 's@-x86_64.pkg.tar.zst.*@@' | sed 's@\\.pkg\\.tar\\.zst.*@@'"
            );
            std::string ver = result.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();
            if (!ver.empty()) Logger::info(_("devtools.status.latest_version") + ver + " (archlinuxcn)");

            if (ver.empty()) {
                Logger::info(_("devtools.status.archlinuxcn_not_found"));
                auto result2 = Executor::shell(
                    "curl " + std::string(curl_opts) +
                    " 'https://build.archlinuxcn.org/packages/#/" + grep_name_ + "' 2>/dev/null | "
                    "grep -oP '" + grep_name_ + "-\\K[0-9.]+' | head -n 1"
                );
                ver = result2.stdout_data;
                while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                    ver.pop_back();
            }
            return ver;
        }
    }

    bool DeveloperTools::download_and_extract_arch_pkg() {
        check_download_path();

        std::string dl_path = download_path_.string();
        const char *curl_opts = "--connect-timeout 15 --max-time 60 -sL";

        Logger::step(_("devtools.step.downloading") + grep_name_ + " ...");

        // 根据版本类型选择镜像源
        std::string mirror_list;
        if (community_edition_) {
            // 社区版 → Arch Linux community 镜像
            mirror_list =
                    "'https://mirrors.kernel.org/archlinux/community/os/x86_64/' "
                    "'https://mirror.rackspace.com/archlinux/community/os/x86_64/' "
                    "'https://mirrors.bfsu.edu.cn/archlinux/community/os/x86_64/'";
        } else {
            // 旗舰版 → archlinuxcn 镜像
            mirror_list =
                    "'https://mirrors.bfsu.edu.cn/archlinuxcn/x86_64/' "
                    "'https://mirrors.tuna.tsinghua.edu.cn/archlinuxcn/x86_64/' "
                    "'https://repo.archlinuxcn.org/x86_64/'";
        }

        // 步骤1: 查找并下载 .pkg.tar.zst (passthrough 显示实时进度)
        auto dl_ret = Executor::passthrough(
            "cd " + dl_path + " && "
            "for url in " + mirror_list + "; do "
            "  echo '🔍 正在扫描: '${url}' ...'; "
            "  LATEST_PKG=$(curl " + std::string(curl_opts) + " \"${url}\" 2>/dev/null | "
            "    grep '\\.pkg\\.tar\\.zst' | grep -Ev '\\.sig' | "
            "    grep -v '\\-jre\\-' | grep '" + grep_name_ + "' | tail -n 1 | "
            "    grep -oP 'href=\"\\K[^\"]+' || true); "
            "  if [ -n \"${LATEST_PKG}\" ]; then "
            "    echo '✅ 找到: '${LATEST_PKG}; "
            "    aria2c --console-log-level=warn --no-conf --continue=true "
            "      --allow-overwrite=true -s 5 -x 5 -k 1M \"${url}${LATEST_PKG}\" && break; "
            "  else "
            "    echo '❌ 此镜像未找到 " + grep_name_ + "'; "
            "  fi; "
            "done"
        );

        // 验证下载结果
        auto pkg_check = Executor::shell(
            "cd " + dl_path + " && ls -t " + grep_name_ + "*.pkg.tar.zst 2>/dev/null | head -n 1"
        );
        std::string pkg_file = pkg_check.stdout_data;
        while (!pkg_file.empty() && (pkg_file.back() == '\n' || pkg_file.back() == '\r'))
            pkg_file.pop_back();

        if (pkg_file.empty()) {
            Logger::error(_("devtools.error.download_not_found_all_mirrors") + grep_name_ + " 的 .pkg.tar.zst");
            Logger::info(_("devtools.hint.manual_download"));
            if (community_edition_) {
                Logger::info("https://www.jetbrains.com/idea/download/#section=linux");
            }
            return false;
        }

        // 验证文件大小
        auto size = fs::file_size(dl_path + "/" + pkg_file);
        if (size == 0) {
            Logger::error(_("devtools.error.pkg_file_empty") + pkg_file);
            fs::remove(dl_path + "/" + pkg_file);
            return false;
        }
        Logger::ok(_("devtools.ok.download_complete") + pkg_file + " (" + std::to_string(size / 1048576) + " MB)");

        std::string pkg_full_path = dl_path + "/" + pkg_file;
        std::string manifest_path = dl_path + "/" + grep_name_ + ".manifest";

        // 步骤2: 生成安装清单并解压到根目录
        Logger::step(_("devtools.step.generating_manifest"));
        Executor::shell("tar -tf '" + pkg_full_path + "' > '" + manifest_path + "' 2>/dev/null");

        Logger::step(_("devtools.step.extracting") + pkg_file + " ...");

        std::string extract_cmd =
                "if command -v pv >/dev/null 2>&1; then "
                "  pv '" + pkg_full_path + "' | tar --use-compress-program zstd -Ppxf - -C / --exclude=.*; "
                "else "
                "  echo '正在后台解压，请稍候...'; "
                "  tar --use-compress-program zstd -Ppxf '" + pkg_full_path + "' -C / --exclude=.*; "
                "fi";

        auto extract_ret = Executor::passthrough(extract_cmd);

        if (!extract_ret.ok()) {
            Logger::error(_("devtools.error.extract_failed_code") + std::to_string(extract_ret.exit_code));
            // 解压失败时删掉不完整的清单文件
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(manifest_path)
                .add_raw("2>/dev/null").build_string());
            return false;
        }

        Logger::ok(_("devtools.ok.extract_done"));

        // 保存版本信息
        std::string latest_ver = fetch_latest_archlinuxcn_version();
        Executor::shell("echo '" + latest_ver + "' > " + dl_path + "/" + grep_name_ + "-version.txt 2>/dev/null");

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // JetBrains API — 官方直链，不依赖 archlinuxcn 抓取
    // ═══════════════════════════════════════════════════════════════════

    std::string DeveloperTools::jetbrains_product_code() const {
        // 映射表: grep_name_ → JetBrains product code
        if (grep_name_ == "intellij-idea-ultimate-edition") return "IIU";
        if (grep_name_ == "intellij-idea-community-edition") return "IIC";
        if (grep_name_ == "pycharm-community-edition") return "PCC";
        if (grep_name_ == "webstorm") return "WS";
        if (grep_name_ == "clion-1") return "CL";
        if (grep_name_ == "goland") return "GO";
        return ""; // 非 JetBrains 产品
    }

    bool DeveloperTools::fetch_jetbrains_link(std::string &out_url,
                                              std::string &out_version) {
        std::string code = jetbrains_product_code();
        if (code.empty()) return false;

        Logger::info(_("devtools.status.checking_jetbrains_api") + grep_name_ + " (code=" + code + ") ...");

        std::string api_url =
                "https://data.services.jetbrains.com/products/releases"
                "?code=" + code + "&latest=true&type=release";

        // 企业级加固：强制IPv4(-4)、3次静默重试、UA伪装
        std::string curl_cmd =
                "curl -4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -s "
                "-A 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36' '"
                + api_url + "' | "
                "jq -r '.[\"" + code + "\"][0] | "
                "\"\\(.version)|\\(.downloads.linux.link)\"'";

        auto result = Executor::shell(curl_cmd);

        std::string data = result.stdout_data;
        while (!data.empty() && (data.back() == '\n' || data.back() == '\r'))
            data.pop_back();

        if (data.empty()) {
            Logger::error(_("devtools.error.jetbrains_api_empty"));
            return false;
        }

        // jq 输出格式: "版本号|下载链接"
        auto sep = data.find('|');
        if (sep == std::string::npos) {
            Logger::error(_("devtools.error.jetbrains_api_parse") + data);
            return false;
        }

        out_version = data.substr(0, sep);
        out_url = data.substr(sep + 1);

        Logger::ok(_("devtools.status.latest_version") + out_version);
        Logger::info(_("devtools.hint.download_url") + out_url);
        return true;
    }

    void DeveloperTools::create_jetbrains_desktop() {
        // 根据 grep_name_ 确定启动脚本名、图标名、显示名
        std::string launcher; // bin/xxx.sh
        std::string icon; // bin/xxx.png
        std::string display_name;
        std::string wm_class;

        if (grep_name_.find("intellij-idea") != std::string::npos) {
            launcher = "bin/idea.sh";
            icon = "bin/idea.png";
            display_name = (community_edition_)
                               ? "IntelliJ IDEA Community Edition"
                               : "IntelliJ IDEA Ultimate Edition";
            wm_class = "jetbrains-idea";
        } else if (grep_name_.find("pycharm") != std::string::npos) {
            launcher = "bin/pycharm.sh";
            icon = "bin/pycharm.png";
            display_name = "PyCharm Community Edition";
            wm_class = "jetbrains-pycharm";
        } else if (grep_name_ == "webstorm") {
            launcher = "bin/webstorm.sh";
            icon = "bin/webstorm.png";
            display_name = "WebStorm";
            wm_class = "jetbrains-webstorm";
        } else if (grep_name_ == "clion-1") {
            launcher = "bin/clion.sh";
            icon = "bin/clion.png";
            display_name = "CLion";
            wm_class = "jetbrains-clion";
        } else if (grep_name_ == "goland") {
            launcher = "bin/goland.sh";
            icon = "bin/goland.png";
            display_name = "GoLand";
            wm_class = "jetbrains-goland";
        } else {
            return; // 不支持的 IDE
        }

        std::string opt_path = "/opt/" + grep_name_;
        std::string exec_path = opt_path + "/" + launcher;
        std::string icon_path = opt_path + "/" + icon;

        // 验证启动脚本存在
        if (!fs::exists(exec_path)) {
            Logger::warn(_("devtools.warn.launcher_not_found") + exec_path);
            return;
        }

        std::string desktop_entry =
                "[Desktop Entry]\n"
                "Name=" + display_name + "\n"
                "Type=Application\n"
                "Comment=JetBrains IDE - installed by tmoes\n"
                "Exec=" + exec_path + " %u\n"
                "Icon=" + icon_path + "\n"
                "Terminal=false\n"
                "StartupWMClass=" + wm_class + "\n"
                "Categories=Development;IDE;\n"
                "MimeType=text/plain;inode/directory;\n"
                "StartupNotify=true\n";

        std::string lnk_dir = apps_lnk_dir_.string();
        std::string desk_file = lnk_dir + "/" + grep_name_ + ".desktop";

        Logger::step(_("devtools.step.creating_desktop") + desk_file);
        Executor::shell("cat > " + desk_file + " <<'EOF'\n" + desktop_entry + "EOF\n"
                        "chmod a+r " + desk_file);

        // 同时创建 /usr/local/bin 下的命令行启动链接
        std::string cli_name;
        if (grep_name_.find("intellij-idea") != std::string::npos)
            cli_name = "idea";
        else if (grep_name_.find("pycharm") != std::string::npos)
            cli_name = "pycharm";
        else if (grep_name_ == "webstorm")
            cli_name = "webstorm";
        else if (grep_name_ == "clion-1")
            cli_name = "clion";
        else if (grep_name_ == "goland")
            cli_name = "goland";

        if (!cli_name.empty()) {
            Executor::shell(CommandBuilder("ln").add_flag("-sf")
                .add_arg(exec_path)
                .add_arg("/usr/local/bin/" + cli_name)
                .add_raw("2>/dev/null || true").build_string());
            Logger::info(_("devtools.status.cli_cmd") + cli_name);
        }

        Logger::ok(_("devtools.ok.desktop_created") + desk_file);
    }

    bool DeveloperTools::download_and_extract_jetbrains(const std::string &url,
                                                        const std::string &filename) {
        check_download_path();
        std::string dl_path = download_path_.string();
        std::string dest = dl_path + "/" + filename;

        // 检查是否已下载
        if (fs::exists(dest) && fs::file_size(dest) > 0) {
            Logger::info("检测到已下载 " + filename +
                         " (" + std::to_string(fs::file_size(dest) / 1048576) + " MB), 跳过");
        } else {
            // 下载 (passthrough 显示实时进度)
            Logger::step(_("devtools.step.downloading") + filename + " ...");
            auto dl_ret = Executor::passthrough(
                "cd " + dl_path + " && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M -o '" + filename + "' '" + url + "'"
            );
            if (!dl_ret.ok() || !fs::exists(dest) || fs::file_size(dest) == 0) {
                Logger::error(_("devtools.error.download_failed_aria2c") + std::to_string(dl_ret.exit_code));
                if (fs::exists(dest)) fs::remove(dest);
                return false;
            }
            Logger::ok(_("devtools.ok.download_complete") + filename + " (" + std::to_string(fs::file_size(dest) / 1048576) + " MB)");
        }

        // 精准获取压缩包内部的根目录名
        auto name_cmd = Executor::shell("tar -tf '" + dest + "' 2>/dev/null | head -1 | cut -f1 -d'/'");
        std::string extracted_dir = name_cmd.stdout_data;
        while (!extracted_dir.empty() && (extracted_dir.back() == '\n' || extracted_dir.back() == '\r')) {
            extracted_dir.pop_back();
        }

        if (extracted_dir.empty()) {
            Logger::error(_("devtools.error.tar_corrupted") + dest);
            return false;
        }

        // 解压到 /opt
        Logger::step(_("devtools.step.extracting") + filename + " ...");

        // 进度条与静默解压的兼容脚本
        std::string extract_cmd =
                "if command -v pv >/dev/null 2>&1; then "
                "  pv '" + dest + "' | tar -zxf - -C /opt; "
                "else "
                "  echo '正在后台解压，请稍候...'; "
                "  tar -zxf '" + dest + "' -C /opt; "
                "fi";

        auto extract_ret = Executor::passthrough(extract_cmd);

        if (!extract_ret.ok()) {
            Logger::error(_("devtools.error.extract_failed_rollback"));
            Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/" + extracted_dir)
                .add_raw("2>/dev/null").build_string());
            return false;
        }

        // 重命名为标准名称以便后续管理
        if (extracted_dir != grep_name_) {
            Logger::info(_("devtools.info.config_dir") + "/opt/" + extracted_dir + " → /opt/" + grep_name_);
            Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/" + grep_name_)
                .add_raw("2>/dev/null").build_string());
            Executor::shell(CommandBuilder("mv")
                .add_arg("/opt/" + extracted_dir)
                .add_arg("/opt/" + grep_name_).build_string());
        }

        // 创建桌面图标
        create_jetbrains_desktop();

        Logger::ok(grep_name_ + _("devtools.ok.install_done"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════

    bool DeveloperTools::download_android_studio(const std::string &url,
                                                 const std::string &filename) {
        check_download_path();
        std::string dest = download_path_.string() + "/" + filename;

        // 文件已存在且非空 → 跳过
        if (fs::exists(dest) && fs::file_size(dest) > 0) {
            Logger::info(_("devtools.detected.latest_downloaded") + filename +
                         " (" + std::to_string(fs::file_size(dest) / 1048576) + " MB), 跳过下载");
            return true;
        }

        Logger::info(_("devtools.hint.download_url") + url);
        Logger::step(_("devtools.step.downloading") + "Android Studio (" + filename + ") ...");

        // passthrough 让 aria2c 进度条实时显示
        auto ret = Executor::passthrough(
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -d " + download_path_.string() +
            " -o " + filename + " -x 10 -s 10 -k 1M '" + url + "'"
        );

        if (!ret.ok()) {
            Logger::error(_("devtools.error.aria2c_return") + std::to_string(ret.exit_code));
            if (fs::exists(dest)) { fs::remove(dest); }
            return false;
        }

        if (!fs::exists(dest) || fs::file_size(dest) == 0) {
            Logger::error(_("devtools.error.file_not_found_after_dl") + dest);
            return false;
        }

        Logger::ok(_("devtools.ok.download_complete") + filename + " (" +
                   std::to_string(fs::file_size(dest) / 1048576) + " MB)");
        return true;
    }

    bool DeveloperTools::extract_android_studio() {
        std::string dl_path = download_path_.string();

        // 查找下载的文件
        auto ls_result = Executor::shell(
            "cd " + dl_path + " && ls -t " + grep_name_ + "*.tar.gz 2>/dev/null | head -n 1"
        );
        std::string pkg_file = ls_result.stdout_data;
        while (!pkg_file.empty() && (pkg_file.back() == '\n' || pkg_file.back() == '\r')) {
            pkg_file.pop_back();
        }

        if (pkg_file.empty()) {
            Logger::error(_("devtools.error.no_install_pkg"));
            return false;
        }

        std::string dest = dl_path + "/" + pkg_file;

        // 精准获取根目录名
        auto name_cmd = Executor::shell("tar -tf '" + dest + "' 2>/dev/null | head -1 | cut -f1 -d'/'");
        std::string extracted_dir = name_cmd.stdout_data;
        while (!extracted_dir.empty() && (extracted_dir.back() == '\n' || extracted_dir.back() == '\r')) {
            extracted_dir.pop_back();
        }

        Logger::step(_("devtools.step.extracting_as"));

        std::string extract_cmd =
                "if command -v pv >/dev/null 2>&1; then "
                "  pv '" + dest + "' | tar -pzxf - -C /opt; "
                "else "
                "  echo '正在后台解压，请稍候...'; "
                "  tar -pzxf '" + dest + "' -C /opt; "
                "fi";

        auto extract_ret = Executor::passthrough(extract_cmd);

        if (!extract_ret.ok()) {
            Logger::error(_("devtools.error.extract_failed_rollback"));
            if (!extracted_dir.empty()) {
                Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/" + extracted_dir)
                .add_raw("2>/dev/null").build_string());
            }
            return false;
        }

        // 标准化目录名
        if (!extracted_dir.empty() && extracted_dir != "android-studio") {
            Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/android-studio")
                .add_raw("2>/dev/null").build_string());
            Executor::shell(CommandBuilder("mv")
                .add_arg("/opt/" + extracted_dir)
                .add_arg("/opt/android-studio").build_string());
        }

        return fs::exists(app_opt_dir_);
    }

    void DeveloperTools::create_android_studio_desktop() {
        std::string lnk_dir = apps_lnk_dir_.string();
        std::string desktop =
                "[Desktop Entry]\n"
                "Name=Android Studio\n"
                "Type=Application\n"
                "Comment=Android Studio provides the fastest tools for building apps on every type of Android device.\n"
                "Exec=/opt/android-studio/bin/studio.sh %F\n"
                "Icon=/opt/android-studio/bin/studio.svg\n"
                "Categories=TextEditor;Development;IDE;\n"
                "MimeType=text/plain;inode/directory;\n"
                "Terminal=false\n"
                "Actions=new-empty-window;\n"
                "StartupNotify=true\n"
                "StartupWMClass=Android-Studio\n";

        Executor::shell("cat > " + lnk_dir + "/android-studio.desktop <<'ASDESKEOF'\n" + desktop + "ASDESKEOF\n" +
                        "chmod a+r " + lnk_dir + "/android-studio.desktop");
    }

    void DeveloperTools::install_java_if_needed() {
        if (Executor::has("java")) {
            Logger::info(_("devtools.status.java_installed"));
            return;
        }
        Logger::step(_("devtools.step.installing_java"));
        auto family = PackageManager::detect_distro_family();
        PackageManager::install("default-jre", family);
    }

    bool DeveloperTools::confirm(const std::string &msg_key, const std::string &fallback) {
        std::string msg = _(msg_key);
        if (msg == msg_key) msg = fallback; // fallback if i18n key missing
        return Logger::confirm(msg);
    }

    void DeveloperTools::show_ide_version_table(const std::string &latest_ver,
                                                const std::string &local_ver) {
        std::string lver = latest_ver.empty() ? "unknown" : latest_ver;
        std::string loc = local_ver.empty() ? "NOT-INSTALLED" : local_ver;

        Logger::info("╔═══╦══════════╦═══════════════════╦════════════════════");
        Logger::info("║   ║          ║                   ║");
        Logger::info("║   ║ software ║    ✨" + _("devtools.table.latest_version") + "     ║   " + _("devtools.table.local_version") + " 🎪");
        Logger::info("║   ║          ║  Latest version   ║  Local version");
        Logger::info("║---║----------║-------------------║--------------------");
        Logger::info("║ 1 ║ " + grep_name_);
        Logger::info("║   ║          ║ " + lver);
        Logger::info("║   ║          ║                   ║ " + loc);
        Logger::info("");
    }

    std::string DeveloperTools::check_local_opt_version() {
        if (app_opt_dir_.empty()) return "";

        if (fs::exists(app_opt_dir_)) {
            // Try to get version from version file
            auto result = Executor::shell(
                "cat " + download_path_.string() + "/" + grep_name_ + "-version.txt 2>/dev/null || echo ''"
            );
            std::string ver = result.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();
            if (!ver.empty()) return ver;
            return "installed";
        }
        return "";
    }

    void DeveloperTools::tip_manual_install(const std::string &url) {
        Logger::info(_("devtools.hint.manual_install_url"));
        Logger::info(url);
    }

    bool DeveloperTools::aria2_download(const std::string &url,
                                        const std::string &dest,
                                        bool continue_on_exists) {
        // 文件已存在且非空 → 跳过
        if (continue_on_exists && fs::exists(dest) && fs::file_size(dest) > 0) {
            Logger::info(_("devtools.detected.already_downloaded") + dest + " (" +
                         std::to_string(fs::file_size(dest) / 1048576) + " MB), 跳过下载");
            return true;
        }

        // 若存在空文件 (上次中断的残留)，先删除
        if (fs::exists(dest) && fs::file_size(dest) == 0) {
            Logger::warn(_("devtools.warn.residual_empty_file") + dest);
            fs::remove(dest);
        }

        Logger::info(_("devtools.hint.download_url") + url);
        Logger::step(_("devtools.step.downloading") + dest + " ...");

        // 💡 核心修复：分离目录和文件名
        std::string dir = fs::path(dest).parent_path().string();
        std::string filename = fs::path(dest).filename().string();

        // 确保下载目录存在
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }

        // 使用 -d 指定目录，-o 指定纯文件名
        auto ret = Executor::passthrough(
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 5 -x 5 -k 1M "
            "-d '" + dir + "' -o '" + filename + "' '" + url + "'"
        );

        // 验证下载结果
        if (!ret.ok()) {
            Logger::error(_("devtools.error.aria2c_return") + std::to_string(ret.exit_code));
            // 清理可能残留的不完整文件
            if (fs::exists(dest)) {
                Logger::warn(_("devtools.warn.deleting_incomplete") + dest);
                fs::remove(dest);
            }
            return false;
        }

        if (!fs::exists(dest) || fs::file_size(dest) == 0) {
            Logger::error(_("devtools.error.file_not_found_after_dl") + dest);
            return false;
        }

        Logger::ok(_("devtools.ok.download_complete") + dest + " (" +
                   std::to_string(fs::file_size(dest) / 1048576) + " MB)");
        return true;
    }

    bool DeveloperTools::download_file(const std::string &url, const std::string &dest_path) {
        return aria2_download(url, dest_path, true);
    }
} // namespace tmoe::domain
