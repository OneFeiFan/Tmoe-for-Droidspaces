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
            Logger::info("检测到您已安装VSCode (官方tar.gz版)");
            Logger::info("请输 code --user-data-dir=${HOME}/.vscode 启动");
            Logger::info("如需卸载: rm -rvf /usr/share/code /usr/share/applications/code.desktop ...");
        } else if (fs::exists("/usr/bin/code")) {
            Logger::info("检测到您已安装VSCode (包管理器版)");
            Logger::info("请输 code --user-data-dir=${HOME}/.vscode 启动");
        }

        // Try running code if available
        if (Executor::has("code")) {
            Executor::shell("code --version --user-data-dir=/tmp/.code 2>/dev/null || true");
        }

        Logger::step("正在准备下载最新版 VS Code...");
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
            Logger::error("当前架构 " + arch + " 暂不支持VS Code官方版");
            return;
        }

        Logger::info("下载链接: " + code_bin_url);

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
                Logger::error("VSCode 下载失败或文件不完整");
                return;
            }
            Executor::passthrough(
                "cd /tmp && "
                "apt-cache show ./VSCODE.deb 2>/dev/null; "
                "dpkg -i ./VSCODE.deb || apt install -y ./VSCODE.deb; "
                "rm -vf VSCODE.deb"
            );
            Logger::ok("安装完成,请输 code --user-data-dir=${HOME}/.vscode 启动");
        } else if (family == DistroFamily::RedHat) {
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "aria2c " + std::string(aria2_flags) + " -o 'VSCODE.rpm' '" + code_bin_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCODE.rpm") || fs::file_size("/tmp/VSCODE.rpm") == 0) {
                Logger::error("VSCode 下载失败或文件不完整");
                return;
            }
            Executor::passthrough("cd /tmp && yum install -y ./VSCODE.rpm; rm -vf VSCODE.rpm");
            Logger::ok("安装完成,请输 code --user-data-dir=${HOME}/.vscode 启动");
        } else {
            // Generic tar.gz install
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "aria2c " + std::string(aria2_flags) + " -o 'VSCODE.tar.gz' '" + code_bin_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCODE.tar.gz") || fs::file_size("/tmp/VSCODE.tar.gz") == 0) {
                Logger::error("VSCode 下载失败或文件不完整");
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
                Logger::warn("VSCode share 文件下载失败，但主程序已安装");
            } else {
                Executor::passthrough(
                    "cd /tmp && tar -Jxvf .VSCODE_USR_SHARE.tar.xz -C /; rm -vf .VSCODE_USR_SHARE.tar.xz"
                );
            }

            // Symlink
            Executor::shell(CommandBuilder("ln").add_flag("-sfv")
                .add_arg("/usr/share/code/bin/code").add_arg("/usr/bin").build_string());
            Logger::ok("安装完成,请输 code --user-data-dir=${HOME}/.vscode 启动");
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
            Logger::error("非常抱歉，Tmoe-linux的开发者未对您的架构进行适配。");
            Logger::info("请选择其它版本");
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
                Logger::info("正在停止服务进程...");
                Executor::shell("pkill node 2>/dev/null || true");
            } else if (ch == "5") vscode_server_remove();

            Logger::press_enter();
        }
    }

    void DeveloperTools::vscode_server_upgrade() {
        Logger::step("正在检测版本信息...");

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
        Logger::info("║   ║ software ║    ✨最新版本     ║   本地版本 🎪");
        Logger::info("║---║----------║-------------------║--------------------");
        Logger::info("║ 1 ║ vscode   ║ " + latest_ver + " ║ " + local_ver);
        Logger::info("║   ║ server   ║                   ║");
        Logger::info("");
        Logger::info("您可以输入 code-server 来启动vscode web服务器。");

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
                Logger::error("无法获取 code-server 下载链接");
                return;
            }

            Logger::info("code-server 下载链接: " + server_url);
            Logger::step("正在下载 code-server ...");

            // 用 passthrough 显示实时进度
            auto dl_ret = Executor::passthrough(
                "cd /tmp && "
                "mkdir -pv .VSCODE_SERVER_TEMP_FOLDER && "
                "cd .VSCODE_SERVER_TEMP_FOLDER && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M -o .VSCODE_SERVER.tar.gz '" + server_url + "'"
            );

            if (!dl_ret.ok()) {
                Logger::error("code-server 下载失败");
                return;
            }

            // 验证下载后文件存在且非空
            if (!fs::exists("/tmp/.VSCODE_SERVER_TEMP_FOLDER/.VSCODE_SERVER.tar.gz") ||
                fs::file_size("/tmp/.VSCODE_SERVER_TEMP_FOLDER/.VSCODE_SERVER.tar.gz") == 0) {
                Logger::error("code-server 下载文件不完整");
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

        Logger::info("若您是初次安装，则请重启code-server");

        // Fix bind address
        Executor::shell(
            "if grep -q '127.0.0.1:8080' \"${HOME}/.config/code-server/config.yaml\" 2>/dev/null; then "
            "sed -i 's@bind-addr:.*@bind-addr: 0.0.0.0:18080@' \"${HOME}/.config/code-server/config.yaml\"; fi"
        );

        Logger::press_enter();
    }

    void DeveloperTools::vscode_server_restart() {
        Logger::info("即将为您启动code-server");
        Logger::info("您之后可以输 code-server 来启动Code Server.");
        Executor::shell("/usr/local/bin/code-server-data/bin/code-server &");

        auto port_result = Executor::shell(
            "grep bind-addr ${HOME}/.config/code-server/config.yaml 2>/dev/null | cut -d ':' -f 3"
        );
        std::string port = port_result.stdout_data;
        if (port.empty()) port = "18080";

        Logger::info("正在为您启动code-server，本机默认访问地址为 localhost:" + port);

        auto ip_result = Executor::shell("ip -4 -br -c a 2>/dev/null | tail -n 1 | cut -d '/' -f 1 | cut -d 'P' -f 2");
        std::string ip = ip_result.stdout_data;
        if (!ip.empty())
            Logger::info("局域网地址: " + ip + ":" + port);

        Logger::info("您可以输 pkill node 来停止进程");
    }

    void DeveloperTools::vscode_server_password() {
        std::string cmd = cfg_.tui_bin +
                          " --inputbox \"请设定访问密码\\nPlease enter the password.\" 12 50 --title \"PASSWORD\"";
        auto result = Executor::passthrough(cmd + " 2>/tmp/vscode_passwd_out");
        // whiptail inputbox is tricky with passthrough; use a simpler approach
        Logger::info("请手动执行: sed -i 's@^password:.*@password: YOUR_PASS@' ~/.config/code-server/config.yaml");
    }

    void DeveloperTools::vscode_server_remove() {
        Logger::info("正在停止code-server进程...");
        Executor::shell("pkill node 2>/dev/null || true");

        Logger::info("按回车键确认移除");
        if (!confirm("devtools.confirm_remove_vscode_server", "确认移除 VS Code Server?"))
            return;

        Executor::shell(CommandBuilder("rm").add_flag("-rvf")
            .add_arg("/usr/local/bin/code-server-data/")
            .add_arg("/usr/local/bin/code-server")
            .add_arg("/tmp/sed-vscode.tmp")
            .add_raw("2>/dev/null").build_string());
        Logger::ok("移除成功");
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
            Logger::error("暂不支持 " + arch + " 架构");
            return;
        }

        // Check existing installation
        if (fs::exists("/usr/bin/codium")) {
            Logger::info("检测到您已安装VSCodium (包管理器版)");
            Logger::info("请输 codium --user-data-dir=${HOME}/.codium 启动");
        } else if (fs::exists("/opt/vscodium-data/codium")) {
            Logger::info("检测到您已安装VSCodium (tar包版)");
            Logger::info("请输 codium 启动");
        }

        // Try to run
        if (Executor::has("codium"))
            Executor::shell("codium --no-sandbox 2>/dev/null &");

        Logger::info("是否下载最新版 VSCodium?");
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
                Logger::error("无法获取 VSCodium 下载链接");
                return;
            }

            Logger::info("VSCodium 下载链接: " + codium_url);

            // 下载 (实时进度)
            auto dl_ret = Executor::passthrough(
                "cd /tmp && aria2c " + std::string(aria2_flags) + " -o 'VSCodium.deb' '" + codium_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCodium.deb") || fs::file_size("/tmp/VSCodium.deb") == 0) {
                Logger::error("VSCodium 下载失败或文件不完整");
                return;
            }

            // 安装
            Executor::passthrough(
                "cd /tmp && apt-cache show ./VSCodium.deb 2>/dev/null; "
                "dpkg -i ./VSCodium.deb; rm -vf VSCodium.deb"
            );
            Logger::ok("安装完成,请输 codium --user-data-dir=${HOME}/.codium 启动");
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
                Logger::error("无法获取 VSCodium 下载链接");
                return;
            }

            Logger::info("VSCodium 下载链接: " + codium_url);

            // 下载 (实时进度)
            auto dl_ret = Executor::passthrough(
                "cd /tmp && aria2c " + std::string(aria2_flags) + " -o 'VSCodium.tar.gz' '" + codium_url + "'"
            );
            if (!dl_ret.ok() || !fs::exists("/tmp/VSCodium.tar.gz") || fs::file_size("/tmp/VSCodium.tar.gz") == 0) {
                Logger::error("VSCodium 下载失败或文件不完整");
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

            Logger::ok("安装完成，请输 codium 启动");
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // 修复 tightvnc 下 vscode 无法启动
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::fix_tightvnc_vscode() {
        Logger::info("本功能仅支持deb系发行版。");
        Logger::info("若无法自动修复，则请手动使用以下命令来启动:");
        Logger::info("env LD_LIBRARY_PATH=${TMOE_LINUX_DIR}/lib codium --user-data-dir=${HOME}/.codium");
        Logger::info("env LD_LIBRARY_PATH=${TMOE_LINUX_DIR}/lib code --user-data-dir=${HOME}/.vscode");

        auto family = PackageManager::detect_distro_family();
        if (family != DistroFamily::Debian) {
            Logger::warn("非Debian系发行版，跳过自动修复");
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
                Logger::error("无法找到 GNU libxcb.so");
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
            Logger::ok("修复完成");
        } else {
            Logger::error("ERROR！无法修复。");
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
            Logger::step("正在检测版本更新信息...");

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
            std::string filename = Executor::shell("basename '" + dl_url + "'").stdout_data;
            while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r')) filename.pop_back();

            if (!download_and_extract_jetbrains(dl_url, filename)) {
                Logger::error("JetBrains IDE 下载或解压失败");
                return;
            }

            // 保存版本信息
            Executor::shell(
                "echo '" + dl_version + "' > " + download_path_.string() + "/" + grep_name_ +
                "-version.txt 2>/dev/null");

            install_java_if_needed();
            Logger::ok(grep_name_ + " " + dl_version + " 安装完成！");
        } else if (grep_name_ == "github-desktop-bin") {
            // ── 转向 GitHub Desktop 官方推荐的 Linux 原生包 ──
            install_github_desktop();
        }
    }

    void DeveloperTools::install_github_desktop() {
        if (cfg_.arch != "amd64") {
            Logger::error("GitHub Desktop (Linux) 仅支持 amd64 (x86_64) 架构");
            Logger::info("官方支持库: https://github.com/shiftkey/desktop");
            return;
        }

        Logger::step("正在通过 GitHub API 获取最新版本...");

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
            Logger::error("获取发布信息失败，可能触发了 GitHub API 速率限制。");
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
            Logger::warn("当前版本未提供 " + ext + "，自动降级为通用 AppImage...");
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
            Logger::error("未找到有效的 GitHub Desktop 下载链接");
            return;
        }

        std::string filename = Executor::shell("basename '" + dl_url + "'").stdout_data;
        while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r')) filename.pop_back();

        check_download_path();
        std::string dest = download_path_.string() + "/" + filename;

        if (!aria2_download(dl_url, dest, true)) {
            Logger::error("下载失败");
            return;
        }

        Logger::step("正在执行原生安装...");

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
        Logger::ok("GitHub Desktop 安装完成！");
    }

    // ═══════════════════════════════════════════════════════════════════
    // install_ide_02 — Android Studio 下载安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_ide_02() {
        icon_file_ = "/opt/android-studio/bin/studio.png";

        Logger::step("正在检测版本更新信息...");
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
            Logger::error("❌ 错误：无法从官方抓取 Android Studio 下载链接，可能网页结构已更改。");
            return;
        }

        // 从链接中提取文件名和版本号
        std::string download_file_name = Executor::shell(
            "basename '" + latest_link + "'"
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
            Logger::error("下载 Android Studio 失败");
            return;
        }

        if (!extract_android_studio()) {
            Logger::error("解压 Android Studio 失败");
            return;
        }

        create_android_studio_desktop();
        install_java_if_needed();

        // 保存版本信息
        Executor::shell("echo '" + latest_ver + "' > " + download_path_.string() +
                        "/" + grep_name_ + "-version.txt 2>/dev/null");

        Logger::ok("Android Studio 安装完成！");
    }

    // ═══════════════════════════════════════════════════════════════════
    // delete_ide_pkg — 删除下载的 tar.zst 安装包
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::delete_ide_pkg() {
        Logger::info("清理下载缓存目录: " + download_path_.string());

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
            Logger::warn("未检测到相关的安装包缓存");
            return;
        }

        Logger::info("发现以下缓存文件:");
        Logger::info(ls_result.stdout_data);

        if (confirm("devtools.confirm_delete_pkg", "确认删除这些安装包吗?")) {
            Executor::shell(
                "cd " + download_path_.string() + " && rm -v " + pattern + ".tar.gz " + pattern + ".deb " + pattern +
                ".rpm " + pattern + ".AppImage 2>/dev/null || true");
            Logger::ok("清理完成");
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
            Logger::warn("检测到安装包不存在");
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
        Logger::warn("  即将卸载: " + grep_name_);
        Logger::warn("═══════════════════════════════════════════");
        Logger::warn("安装目录 : " + app_opt_dir_);
        Logger::warn("桌面图标 : " + lnk_dir + "/" + lnk_file_);
        if (!cli_link.empty())
            Logger::warn("CLI 链接  : " + cli_link);
        Logger::warn("版本记录 : " + version_file);
        if (fs::exists(manifest_file))
            Logger::warn("安装清单 : " + manifest_file + " (将触发深度清理)");

        if (!confirm("devtools.confirm_remove_ide", "确认卸载 " + grep_name_ + " ?"))
            return;

        // 基于 Manifest 的安全深度清理
        if (fs::exists(manifest_file)) {
            Logger::step("正在根据清单执行系统级深度清理...");
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
            Logger::step("正在移除 GitHub Desktop...");
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

        Logger::ok(grep_name_ + " 已彻底卸载");
    }

    // ═══════════════════════════════════════════════════════════════════
    // remove_ide_02 — 卸载 Android Studio
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::remove_ide_02() {
        std::string lnk_dir = apps_lnk_dir_.string();
        std::string dl_path = download_path_.string();
        std::string version_file = dl_path + "/" + grep_name_ + "-version.txt";

        Logger::warn("═══════════════════════════════════════════");
        Logger::warn("  即将卸载: " + grep_name_);
        Logger::warn("═══════════════════════════════════════════");
        Logger::warn("安装目录 : " + app_opt_dir_);
        Logger::warn("桌面图标 : " + lnk_dir + "/" + lnk_file_);
        Logger::warn("版本记录 : " + version_file);

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

        Logger::ok(grep_name_ + " 已卸载");
    }

    // ═══════════════════════════════════════════════════════════════════
    // GNU Emacs — 快速安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_emacs() {
        Logger::step("安装 GNU Emacs...");
        auto family = PackageManager::detect_distro_family();
        PackageManager::install("emacs", family);
        Logger::ok("Emacs 安装完成");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Code::Blocks — 快速安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_code_blocks() {
        Logger::step("安装 Code::Blocks...");
        auto family = PackageManager::detect_distro_family();
        PackageManager::install("codeblocks", family);
        Logger::ok("Code::Blocks 安装完成");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Sublime Text — 添加官方源 + 安装
    // ═══════════════════════════════════════════════════════════════════
    void DeveloperTools::install_sublime_text() {
        std::string arch = cfg_.arch;
        if (arch != "amd64" && arch != "i386") {
            Logger::error("Sublime Text 仅支持 amd64/i386 架构");
            return;
        }

        auto family = PackageManager::detect_distro_family();

        switch (family) {
            case DistroFamily::Debian: {
                Logger::step("添加 Sublime Text 官方 GPG 密钥和源...");
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

        Logger::ok("Sublime Text 安装完成");
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
            Logger::info("正在查询 Arch community 仓库: " + grep_name_ + " ...");
            auto result = Executor::shell(
                "curl " + std::string(curl_opts) +
                " 'https://archlinux.org/packages/community/x86_64/" + grep_name_ + "/' 2>/dev/null | "
                "grep -oP '(?<=<meta itemprop=\"softwareVersion\" content=\")[^\"]+' | head -n 1"
            );
            std::string ver = result.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();

            if (!ver.empty()) {
                Logger::info("最新版本: " + ver + " (Arch community)");
                return ver;
            }

            // 回退：直接扫 community 镜像目录列表
            Logger::info("包页面解析失败，尝试扫描镜像目录...");
            auto fallback = Executor::shell(
                "curl " + std::string(curl_opts) +
                " 'https://mirrors.kernel.org/archlinux/community/os/x86_64/' 2>/dev/null | "
                "grep -oP '" + grep_name_ + "-\\K[0-9][0-9._-]+(?=-x86_64\\.pkg\\.tar\\.zst)' | "
                "sort -V | tail -n 1"
            );
            ver = fallback.stdout_data;
            while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r'))
                ver.pop_back();
            if (!ver.empty()) Logger::info("最新版本: " + ver + " (Arch community mirror)");
            return ver;
        } else {
            // ── 旗舰版/AUR → archlinuxcn 仓库 ──
            Logger::info("正在查询 archlinuxcn 仓库: " + grep_name_ + " ...");
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
            if (!ver.empty()) Logger::info("最新版本: " + ver + " (archlinuxcn)");

            if (ver.empty()) {
                Logger::info("archlinuxcn 未找到，尝试 build.archlinuxcn.org ...");
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

        Logger::step("正在下载 " + grep_name_ + " ...");

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
            Logger::error("下载失败：在所有镜像源均未找到 " + grep_name_ + " 的 .pkg.tar.zst");
            Logger::info("请前往官网手动下载安装。");
            if (community_edition_) {
                Logger::info("https://www.jetbrains.com/idea/download/#section=linux");
            }
            return false;
        }

        // 验证文件大小
        auto size = fs::file_size(dl_path + "/" + pkg_file);
        if (size == 0) {
            Logger::error("下载的包文件为空: " + pkg_file);
            fs::remove(dl_path + "/" + pkg_file);
            return false;
        }
        Logger::ok("下载完成: " + pkg_file + " (" + std::to_string(size / 1048576) + " MB)");

        std::string pkg_full_path = dl_path + "/" + pkg_file;
        std::string manifest_path = dl_path + "/" + grep_name_ + ".manifest";

        // 步骤2: 生成安装清单并解压到根目录
        Logger::step("正在生成安装清单 (Manifest)...");
        Executor::shell("tar -tf '" + pkg_full_path + "' > '" + manifest_path + "' 2>/dev/null");

        Logger::step("正在解压 " + pkg_file + " ...");

        std::string extract_cmd =
                "if command -v pv >/dev/null 2>&1; then "
                "  pv '" + pkg_full_path + "' | tar --use-compress-program zstd -Ppxf - -C / --exclude=.*; "
                "else "
                "  echo '正在后台解压，请稍候...'; "
                "  tar --use-compress-program zstd -Ppxf '" + pkg_full_path + "' -C / --exclude=.*; "
                "fi";

        auto extract_ret = Executor::passthrough(extract_cmd);

        if (!extract_ret.ok()) {
            Logger::error("解压失败: tar.zst 返回错误码 " + std::to_string(extract_ret.exit_code));
            // 解压失败时删掉不完整的清单文件
            Executor::shell(CommandBuilder("rm").add_flag("-f").add_arg(manifest_path)
                .add_raw("2>/dev/null").build_string());
            return false;
        }

        Logger::ok("解压完成");

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

        Logger::info("正在通过 JetBrains API 查询 " + grep_name_ + " (code=" + code + ") ...");

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
            Logger::error("JetBrains API 返回空数据或请求超时，请检查网络");
            return false;
        }

        // jq 输出格式: "版本号|下载链接"
        auto sep = data.find('|');
        if (sep == std::string::npos) {
            Logger::error("JetBrains API 解析失败: " + data);
            return false;
        }

        out_version = data.substr(0, sep);
        out_url = data.substr(sep + 1);

        Logger::ok("最新版本: " + out_version);
        Logger::info("下载链接: " + out_url);
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
            Logger::warn("未找到启动脚本，跳过桌面图标创建: " + exec_path);
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

        Logger::step("创建桌面图标: " + desk_file);
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
            Logger::info("命令行启动: " + cli_name);
        }

        Logger::ok("桌面图标已创建: " + desk_file);
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
            Logger::step("正在下载 " + filename + " ...");
            auto dl_ret = Executor::passthrough(
                "cd " + dl_path + " && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -s 5 -x 5 -k 1M -o '" + filename + "' '" + url + "'"
            );
            if (!dl_ret.ok() || !fs::exists(dest) || fs::file_size(dest) == 0) {
                Logger::error("下载失败或文件为空: aria2c 返回 " + std::to_string(dl_ret.exit_code));
                if (fs::exists(dest)) fs::remove(dest);
                return false;
            }
            Logger::ok("下载完成: " + filename + " (" + std::to_string(fs::file_size(dest) / 1048576) + " MB)");
        }

        // 精准获取压缩包内部的根目录名
        auto name_cmd = Executor::shell("tar -tf '" + dest + "' 2>/dev/null | head -1 | cut -f1 -d'/'");
        std::string extracted_dir = name_cmd.stdout_data;
        while (!extracted_dir.empty() && (extracted_dir.back() == '\n' || extracted_dir.back() == '\r')) {
            extracted_dir.pop_back();
        }

        if (extracted_dir.empty()) {
            Logger::error("无法读取压缩包内容，包可能已损坏: " + dest);
            return false;
        }

        // 解压到 /opt
        Logger::step("正在解压 " + filename + " ...");

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
            Logger::error("解压失败，正在执行回滚清理...");
            Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/" + extracted_dir)
                .add_raw("2>/dev/null").build_string());
            return false;
        }

        // 重命名为标准名称以便后续管理
        if (extracted_dir != grep_name_) {
            Logger::info("配置目录: /opt/" + extracted_dir + " → /opt/" + grep_name_);
            Executor::shell(CommandBuilder("rm").add_flag("-rf")
                .add_arg("/opt/" + grep_name_)
                .add_raw("2>/dev/null").build_string());
            Executor::shell(CommandBuilder("mv")
                .add_arg("/opt/" + extracted_dir)
                .add_arg("/opt/" + grep_name_).build_string());
        }

        // 创建桌面图标
        create_jetbrains_desktop();

        Logger::ok(grep_name_ + " 安装完成");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════

    bool DeveloperTools::download_android_studio(const std::string &url,
                                                 const std::string &filename) {
        check_download_path();
        std::string dest = download_path_.string() + "/" + filename;

        // 文件已存在且非空 → 跳过
        if (fs::exists(dest) && fs::file_size(dest) > 0) {
            Logger::info("检测到您已经下载最新版 " + filename +
                         " (" + std::to_string(fs::file_size(dest) / 1048576) + " MB), 跳过下载");
            return true;
        }

        Logger::info("下载链接: " + url);
        Logger::step("正在下载 Android Studio (" + filename + ") ...");

        // passthrough 让 aria2c 进度条实时显示
        auto ret = Executor::passthrough(
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -d " + download_path_.string() +
            " -o " + filename + " -x 10 -s 10 -k 1M '" + url + "'"
        );

        if (!ret.ok()) {
            Logger::error("aria2c 返回错误码 " + std::to_string(ret.exit_code));
            if (fs::exists(dest)) { fs::remove(dest); }
            return false;
        }

        if (!fs::exists(dest) || fs::file_size(dest) == 0) {
            Logger::error("下载后文件不存在或为空: " + dest);
            return false;
        }

        Logger::ok("下载完成: " + filename + " (" +
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
            Logger::error("找不到下载的安装包");
            return false;
        }

        std::string dest = dl_path + "/" + pkg_file;

        // 精准获取根目录名
        auto name_cmd = Executor::shell("tar -tf '" + dest + "' 2>/dev/null | head -1 | cut -f1 -d'/'");
        std::string extracted_dir = name_cmd.stdout_data;
        while (!extracted_dir.empty() && (extracted_dir.back() == '\n' || extracted_dir.back() == '\r')) {
            extracted_dir.pop_back();
        }

        Logger::step("正在解压 Android Studio 到 /opt ...");

        std::string extract_cmd =
                "if command -v pv >/dev/null 2>&1; then "
                "  pv '" + dest + "' | tar -pzxf - -C /opt; "
                "else "
                "  echo '正在后台解压，请稍候...'; "
                "  tar -pzxf '" + dest + "' -C /opt; "
                "fi";

        auto extract_ret = Executor::passthrough(extract_cmd);

        if (!extract_ret.ok()) {
            Logger::error("解压失败，正在执行回滚清理...");
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
            Logger::info("Java 已安装，跳过");
            return;
        }
        Logger::step("安装 Java 运行时 (JetBrains IDE 依赖)...");
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
        Logger::info("║   ║ software ║    ✨最新版本     ║   本地版本 🎪");
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
        Logger::info("如需手动安装，请前往官网:");
        Logger::info(url);
    }

    bool DeveloperTools::aria2_download(const std::string &url,
                                        const std::string &dest,
                                        bool continue_on_exists) {
        // 文件已存在且非空 → 跳过
        if (continue_on_exists && fs::exists(dest) && fs::file_size(dest) > 0) {
            Logger::info("检测到已下载 " + dest + " (" +
                         std::to_string(fs::file_size(dest) / 1048576) + " MB), 跳过下载");
            return true;
        }

        // 若存在空文件 (上次中断的残留)，先删除
        if (fs::exists(dest) && fs::file_size(dest) == 0) {
            Logger::warn("检测到残留空文件，删除后重新下载: " + dest);
            fs::remove(dest);
        }

        Logger::info("下载链接: " + url);
        Logger::step("正在下载 " + dest + " ...");

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
            Logger::error("aria2c 返回错误码 " + std::to_string(ret.exit_code));
            // 清理可能残留的不完整文件
            if (fs::exists(dest)) {
                Logger::warn("删除不完整的下载文件: " + dest);
                fs::remove(dest);
            }
            return false;
        }

        if (!fs::exists(dest) || fs::file_size(dest) == 0) {
            Logger::error("下载后文件不存在或为空: " + dest);
            return false;
        }

        Logger::ok("下载完成: " + dest + " (" +
                   std::to_string(fs::file_size(dest) / 1048576) + " MB)");
        return true;
    }

    bool DeveloperTools::download_file(const std::string &url, const std::string &dest_path) {
        return aria2_download(url, dest_path, true);
    }
} // namespace tmoe::domain
