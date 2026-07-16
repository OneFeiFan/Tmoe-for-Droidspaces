#include "domain/apps/software_center.h"
#include "domain/apps/media_tools.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    SoftwareCenter::SoftwareCenter(const TmoeConfig &cfg) : cfg_(cfg),
                                                            media_tools_(std::make_unique<MediaTools>(cfg)) {
    }

    SoftwareCenter::~SoftwareCenter() = default;

    // ═══════════════════════════════════════════════════════════════
    // 3. ⚛️ Electron Apps — 对应旧 Bash tmoe_electron_repo → sources/electron-apps
    //    所有应用从 https://packages.tmoe.me/apps/<name>/app.tar.xz 下载
    // ═══════════════════════════════════════════════════════════════

    // ═══════════════════════════════════
    // 通用: 安装或卸载子菜单
    // ═══════════════════════════════════
    void SoftwareCenter::electron_install_or_remove(const std::string &app_name) {
        std::string menu = cfg_.tui_bin + " --title \"" + app_name + " manager\""
                           " --menu \"" + _f("swcenter.electron.what_to_do", app_name) + "\" 0 0 0 "
                           "\"1\" \"" + _("swcenter.electron.install_upgrade") + "\" "
                           "\"2\" \"" + _("swcenter.electron.remove") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") {
            ensure_electron_runtime();
            download_tmoe_electron_app(app_name);
        } else if (ch == "2") {
            remove_electron_app(app_name);
        }
    }

    // ═══════════════════════════════════
    // Electron 运行时
    // ═══════════════════════════════════
    void SoftwareCenter::ensure_electron_runtime() {
        if (fs::exists("/opt/electron/electron")) return;

        Logger::step(_("swcenter.electron.install_runtime"));
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-pv").add_arg("/opt/electron").execute();

        // 下载最新 Electron
        auto result = Executor::passthrough(
            "cd /tmp && "
            "ELECTRON_VER=18.2.3; "
            "ELECTRON_URL=\"https://npmmirror.com/mirrors/electron/v${ELECTRON_VER}/electron-v${ELECTRON_VER}-linux-x64.zip\"; "
            "rm -rf .electron_tmp 2>/dev/null; mkdir -pv .electron_tmp; cd .electron_tmp; "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 3 -x 3 -k 1M -o electron.zip \"${ELECTRON_URL}\" && "
            "sudo unzip -qo electron.zip -d /opt/electron && "
            "sudo chmod a+rx /opt/electron/electron && "
            "sudo ln -sf /opt/electron/electron /usr/bin/electron"
        );

        if (!result.ok() || !fs::exists("/opt/electron/electron")) {
            Logger::warn(_("swcenter.electron.runtime_failed"));
        } else {
            Logger::ok(_("swcenter.electron.runtime_installed"));
        }
        CommandBuilder("rm").add_flag("-rf").add_arg("/tmp/.electron_tmp").add_raw("2>/dev/null").execute();
    }

    // ═══════════════════════════════════
    // 下载 tmoe electron app
    // ═══════════════════════════════════
    void SoftwareCenter::download_tmoe_electron_app(const std::string &app_name) {
        const std::string pkg_url = "https://packages.tmoe.me";
        std::string tmp_dir = "/tmp/." + app_name + "_TEMP_FOLDER";

        CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
        CommandBuilder("mkdir").add_flag("-pv").add_arg(tmp_dir).execute();
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-pv").add_arg("/opt").execute();

        Logger::step(_f("swcenter.electron.downloading", app_name));
        auto dl_ret = Executor::passthrough(
            "cd " + tmp_dir + " && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -x 3 -k 1M --split 3 -o app.tar.xz "
            "'" + pkg_url + "/apps/" + app_name + "/app.tar.xz'"
        );

        if (!dl_ret.ok()) {
            Logger::error(_f("swcenter.electron.download_failed", app_name));
            CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
            return;
        }

        Logger::step(_f("swcenter.electron.extracting", app_name));
        Executor::passthrough("cd " + tmp_dir + " && sudo tar -Jxvf app.tar.xz -C /opt");

        if (!fs::exists("/opt/" + app_name)) {
            Logger::warn(_f("swcenter.electron.extract_not_found", app_name));
            // 重试一次
            Executor::passthrough(
                "cd " + tmp_dir + " && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -x 3 -k 1M --split 3 -o app.tar.xz "
                "'" + pkg_url + "/apps/" + app_name + "/app.tar.xz' && "
                "sudo tar -Jxvf app.tar.xz -C /opt"
            );
        }

        if (!fs::exists("/opt/" + app_name)) {
            Logger::error(_f("swcenter.electron.install_failed", app_name));
            CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
            return;
        }

        // 修复权限并复制 .desktop 文件
        Executor::shell(
            "sudo chmod -Rv 755 /opt/" + app_name + "/usr/bin/ 2>/dev/null || true; "
            "find /opt/" + app_name + " -type d -print | xargs sudo chmod -v a+rx 2>/dev/null || true; "
            "find /opt/" + app_name + " -type f -print | xargs sudo chmod a+r 2>/dev/null || true"
        );

        // 复制 .desktop 到系统
        Executor::shell(
            "cd /opt/" + app_name + " && "
            "if [ -e ." + apps_lnk_dir_ + "/" + app_name + ".desktop ]; then "
            "  sudo cp -vf ." + apps_lnk_dir_ + "/" + app_name + ".desktop " + apps_lnk_dir_ + "/; "
            "fi"
        );

        CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
        Logger::ok(_f("swcenter.electron.install_done", app_name));
    }

    void SoftwareCenter::remove_electron_app(const std::string &app_name) {
        Logger::warn(_f("swcenter.electron.will_remove", app_name));
        Logger::warn("  rm -rv /opt/" + app_name + " " + apps_lnk_dir_ + "/" + app_name + ".desktop");
        if (!Logger::confirm(_f("swcenter.electron.confirm_remove", app_name)))
            return;
        CommandBuilder("sudo").add_arg("rm").add_flag("-rf")
                .add_arg("/opt/" + app_name)
                .add_arg(apps_lnk_dir_ + "/" + app_name + ".desktop")
                .add_raw("2>/dev/null").execute();
        Logger::ok(_f("swcenter.electron.removed", app_name));
    }

    // ═══════════════════════════════════════════════════════════════
    // 4. 🎶 Multimedia — 对应旧 Bash tmoe_multimedia_menu (12项)
    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    // install_with_check — 模拟 Bash beta_features_quick_install
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::install_with_check(const std::string &pkg, DistroFamily family) {
        if (Executor::has(pkg)) {
            Logger::info(_f("swcenter.already_installed", pkg));
            if (!Logger::confirm_yes_default(_f("swcenter.reinstall_confirm", pkg)))
                return;
        }
        Logger::step(_f("swcenter.installing", pkg));
        PackageManager::install(pkg, family);
    }

    // ═══════════════════════════════════════════════════════════════
    // install_spotify — 还原 Bash install_spotify (line 502-532)
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::install_spotify() {
        if (cfg_.arch != "amd64") {
            Logger::error(_("swcenter.spotify.arch_unsupported"));
            return;
        }

        Logger::info("https://www.spotify.com/tw/download/linux/");
        auto family = infer_family_from_config(cfg_.linux_distro);

        if (family == DistroFamily::Debian) {
            Logger::step(_("swcenter.spotify.installing"));
            // 下载并安装 GPG 公钥
            Executor::passthrough(
                "curl -LsS https://download.spotify.com/debian/pubkey.gpg | "
                "gpg --dearmor > /tmp/spotify.gpg && "
                "sudo install -o root -g root -m 644 /tmp/spotify.gpg "
                "/etc/apt/trusted.gpg.d/spotify.gpg && rm -f /tmp/spotify.gpg");
            Executor::passthrough(
                "curl -LsS https://download.spotify.com/debian/pubkey_0D811D58.gpg | "
                "gpg --dearmor > /tmp/spotify_pub.gpg && "
                "sudo install -o root -g root -m 644 /tmp/spotify_pub.gpg "
                "/etc/apt/trusted.gpg.d/spotify_pub.gpg && rm -f /tmp/spotify_pub.gpg");
            // 添加 apt 源
            SystemHelper::write_file("/etc/apt/sources.list.d/spotify.list",
                                     "deb http://repository.spotify.com stable non-free\n");
            Logger::info(_("swcenter.spotify.uninstall_hint"));
            PackageManager::update(family);
            PackageManager::install("spotify-client", family);
        } else if (family == DistroFamily::Arch) {
            Logger::info(_("swcenter.spotify.arch_hint"));
            PackageManager::install("spotify", family);
        } else {
            Logger::info(_("swcenter.spotify.use_snap"));
            Executor::passthrough("sudo snap install spotify");
        }
        Logger::ok(_("swcenter.spotify.install_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // install_netease_cloud_music — 还原 Bash (line 1030-1098)
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::install_netease_cloud_music() {
        auto family = infer_family_from_config(cfg_.linux_distro);

        // 下载图标
        std::string icon_dir = cfg_.work_dir.string() + "/icons";
        std::string icon_file = icon_dir + "/netease-cloud-music.jpg";
        if (!fs::exists(icon_file)) {
            Executor::shell("sudo mkdir -pv " + icon_dir);
            Executor::passthrough(
                "sudo aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                "-d " + icon_dir + " -o netease-cloud-music.jpg "
                "\"https://gitee.com/ak2/icons/raw/master/netease-cloud-music.jpg\"");
        }

        Logger::step(_("swcenter.netease.checking_version"));
        Logger::info(_("swcenter.netease.manual_hint") + std::string(": https://music.163.com/st/download"));

        // 从优麒麟仓库获取版本号
        std::string kylin_repo = "http://archive.ubuntukylin.com/software/pool/partner/";
        auto ver_result = Executor::shell(
            "curl -sL '" + kylin_repo + "' 2>/dev/null | "
            "grep netease-cloud-music | grep -oP 'href=\"\\K[^\"]+' | head -1");
        std::string latest_ver = ver_result.stdout_data;
        while (!latest_ver.empty() && (latest_ver.back() == '\n' || latest_ver.back() == '\r'))
            latest_ver.pop_back();

        if (!latest_ver.empty())
            Logger::info(_f("swcenter.netease.version_detected", latest_ver));

        // 架构检查
        if (cfg_.arch != "amd64" && cfg_.arch != "i386" && family != DistroFamily::Arch) {
            Logger::error(_("swcenter.netease.arch_unsupported"));
            return;
        }

        if (family == DistroFamily::Arch) {
            PackageManager::install("netease-cloud-music", family);
        } else if (family == DistroFamily::RedHat) {
            Executor::passthrough("curl -LsS https://dl.senorsen.com/pub/package/linux/add_repo.sh | sudo bash");
            Executor::passthrough("sudo dnf install -y "
                "http://dl-http.senorsen.com/pub/package/linux/rpm/senorsen-repo-0.0.1-1.noarch.rpm");
            Executor::passthrough("sudo dnf install -y netease-cloud-music");
        } else {
            // Debian 系：从优麒麟或debiancn下载deb
            std::string deb_url;
            if (cfg_.arch == "amd64") {
                deb_url = kylin_repo;
            } else {
                deb_url = "http://mirrors.ustc.edu.cn/debiancn/pool/main/n/netease-cloud-music/";
            }
            Executor::passthrough(
                "cd /tmp && "
                "LATEST=$(curl -sL '" + deb_url + "' 2>/dev/null | "
                "  grep netease-cloud-music | grep -oP 'href=\"\\K[^\"]+' | tail -1) && "
                "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                "  -s 5 -x 5 -k 1M -o netease-cloud-music.deb "
                "  \"" + deb_url + "${LATEST}\" && "
                "sudo apt install -y ./netease-cloud-music.deb || sudo apt install -f -y && "
                "rm -f netease-cloud-music.deb");
        }

        // 保存版本信息
        std::string ver_path = cfg_.work_dir.string() + "/netease-cloud-music-version";
        Executor::shell("echo '" + latest_ver + "' | sudo tee " + ver_path + " > /dev/null");
        Logger::ok(_("swcenter.netease.install_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // 5. 🐧 SNS — 对应旧 Bash tmoe_social_network_service (10项)
    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    // 5. 🐧 SNS — 对应旧 Bash tmoe_social_network_service
    //    注意: LinuxQQ/WeChat/Skype/Mitalk 不在标准仓库，需特殊下载
    //    Thunderbird/Kmail/Evolution/Empathy/Pidgin/Xchat 在标准仓库
    // ═══════════════════════════════════════════════════════════════

    void SoftwareCenter::install_linux_qq() {
        Logger::step(_("swcenter.qq.fetching_version"));
        Logger::info(_("swcenter.qq.manual_download_hint"));

        auto family = PackageManager::detect_distro_family();

        // 1. Arch Linux 专属特权：直接走官方包管理器 (AUR) 最为优雅稳定
        if (family == DistroFamily::Arch) {
            Logger::info(_("swcenter.qq.arch_detected"));
            PackageManager::install("linuxqq", family);
            Logger::ok(_("swcenter.qq.transferred"));
            return;
        }

        // 2. 安全落地配置文件：使用官方动态分发接口
        std::string tmp_json = "/tmp/qq_pc_config.json";
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        CommandBuilder("curl")
                .add_raw(std::string(curl_opts))
                .add_arg("https://cdn-go.cn/qq-web/im.qq.com_new/latest/rainbow/pcConfig.json")
                .add_opt("-o", tmp_json)
                .execute();

        if (Executor::shell("grep '\"Linux\"' " + tmp_json + " > /dev/null 2>&1").exit_code != 0) {
            Logger::error(_("swcenter.qq.fetch_config_failed"));
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();
            return;
        }

        // 3. 提取版本号
        std::string version = CommandBuilder("jq").add_flag("-r").add_arg(".Linux.version").add_arg(tmp_json).execute().
                stdout_data;
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();

        if (version.empty()) version = _("swcenter.qq.unknown_version");

        if (!Logger::confirm(_f("swcenter.qq.confirm_install", version))) {
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();
            return;
        }

        // 4. 智能匹配架构与包格式
        std::string arch = cfg_.arch;
        std::string format_key = "appimage";
        std::string ext = ".AppImage";

        if (family == DistroFamily::Debian) {
            format_key = "deb";
            ext = ".deb";
        } else if (family == DistroFamily::RedHat) {
            format_key = "rpm";
            ext = ".rpm";
        }

        // 组装 jq 提取命令
        std::string jq_cmd;
        if (arch == "amd64") {
            jq_cmd = "jq -r '.Linux.x64DownloadUrl." + format_key + "' " + tmp_json;
        } else if (arch == "arm64") {
            jq_cmd = "jq -r '.Linux.armDownloadUrl." + format_key + "' " + tmp_json;
        } else if (arch == "loongarch64") {
            jq_cmd = "jq -r '.Linux.loongarchDownloadUrl' " + tmp_json;
            format_key = "deb";
            ext = ".deb"; // 龙芯目前官方仅供 deb
        } else if (arch == "mips64el") {
            jq_cmd = "jq -r '.Linux.mipsDownloadUrl' " + tmp_json;
            format_key = "deb";
            ext = ".deb"; // MIPS 目前官方仅供 deb
        } else {
            Logger::error(_f("swcenter.qq.arch_unsupported", arch));
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();
            return;
        }

        std::string dl_url = Executor::shell(jq_cmd).stdout_data;
        while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();

        // 柔性降级策略：如果当前架构缺失指定的 deb/rpm，自动降级为全系统通用的 AppImage
        if (dl_url.empty() || dl_url == "null") {
            Logger::warn(_f("swcenter.qq.no_native_pkg", format_key));
            format_key = "appimage";
            ext = ".AppImage";

            if (arch == "amd64") jq_cmd = "jq -r '.Linux.x64DownloadUrl.appimage' " + tmp_json;
            else if (arch == "arm64") jq_cmd = "jq -r '.Linux.armDownloadUrl.appimage' " + tmp_json;

            dl_url = Executor::shell(jq_cmd).stdout_data;
            while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();
        }

        // 用完即焚，销毁临时 JSON
        CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();

        if (dl_url.empty() || dl_url == "null") {
            Logger::error(_("swcenter.qq.fetch_dl_failed"));
            return;
        }

        Logger::info(_f("swcenter.qq.dl_link_resolved", dl_url));
        Logger::step(_("swcenter.qq.downloading_pkg"));

        // 5. 下载包体 (使用独立 -o 避免路径拼接陷阱)
        std::string dest_file = "LINUXQQ" + ext;
        std::string download_cmd = "cd /tmp && aria2c --console-log-level=warn --no-conf --continue=true "
                                   "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dest_file + " '" + dl_url + "'";

        if (!Executor::passthrough(download_cmd).ok()) {
            Logger::error(_("swcenter.qq.dl_failed"));
            return;
        }

        // 6. 执行系统级原生安装
        Logger::step(_("swcenter.qq.installing"));
        if (ext == ".deb") {
            Executor::passthrough(
                "cd /tmp && apt-cache show ./" + dest_file + " 2>/dev/null; sudo apt install -y ./" + dest_file +
                " && rm -vf " + dest_file);
        } else if (ext == ".rpm") {
            Executor::passthrough(
                "cd /tmp && sudo rpm -Uvh ./" + dest_file + " || sudo yum localinstall -y ./" + dest_file +
                " && rm -vf " +
                dest_file);
        } else {
            // AppImage 配置部署
            Executor::shell(
                "sudo mkdir -p /opt/linuxqq && sudo mv /tmp/" + dest_file +
                " /opt/linuxqq/linuxqq.AppImage && sudo chmod +x /opt/linuxqq/linuxqq.AppImage");
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=QQ\n"
                    "Comment=Tencent QQ\n"
                    "Exec=/opt/linuxqq/linuxqq.AppImage --no-sandbox %U\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Icon=qq\n"
                    "Categories=Network;InstantMessaging;\n";
            Executor::shell(
                "sudo sh -c 'cat > /usr/share/applications/linuxqq.desktop' <<'EOF'\n" + desktop_content + "EOF\n");
        }

        Logger::ok(_f("swcenter.qq.install_done", version));
    }

    void SoftwareCenter::install_wechat() {
        Logger::step(_("swcenter.wechat.fetching_version"));
        Logger::info(_("swcenter.wechat.data_source"));

        auto family = PackageManager::detect_distro_family();

        // 1. Arch Linux 专属特权：AUR 的 linux-weixin 是由社区维护的官方重打包版本
        if (family == DistroFamily::Arch) {
            Logger::info(_("swcenter.wechat.arch_detected"));
            PackageManager::install("linux-weixin", family);
            Logger::ok(_("swcenter.wechat.transferred"));
            return;
        }

        // 2. 安全策略：获取官网 HTML 源码落地为临时文件
        std::string tmp_html = "/tmp/wechat_linux.html";
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        CommandBuilder("curl")
                .add_raw(std::string(curl_opts))
                .add_arg("https://linux.weixin.qq.com/")
                .add_opt("-o", tmp_html)
                .execute();

        if (!fs::exists(tmp_html) || fs::file_size(tmp_html) == 0) {
            Logger::error(_("swcenter.wechat.fetch_page_failed"));
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_html).add_raw("2>/dev/null").execute();
            return;
        }

        // 3. 智能匹配架构与包格式
        std::string arch = cfg_.arch;
        std::string format_key = "deb"; // 默认首选格式

        if (family == DistroFamily::Debian) format_key = "deb";
        else if (family == DistroFamily::RedHat) format_key = "rpm";

        // 构建正则表达式的架构关键词字典
        std::string arch_grep;
        if (arch == "amd64") {
            arch_grep = "amd64|x86_64";
        } else if (arch == "arm64") {
            arch_grep = "arm64|aarch64";
        } else if (arch == "loongarch64") {
            arch_grep = "loongarch64";
        } else if (arch == "mips64el") {
            arch_grep = "mips64el";
        } else {
            Logger::error(_f("swcenter.wechat.arch_unsupported", arch));
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_html).add_raw("2>/dev/null").execute();
            return;
        }

        // 4. 解析下载直链
        std::string parse_cmd = "grep -oP 'href=\"\\K[^\"]+' " + tmp_html +
                                " | grep -iE '(" + arch_grep + ")' | grep '\\." + format_key + "$' | head -n 1";

        std::string dl_url = Executor::shell(parse_cmd).stdout_data;
        while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();

        // 柔性降级：如果未获取到指定的 rpm/deb，尝试兜底抓取 AppImage
        if (dl_url.empty()) {
            parse_cmd = "grep -oP 'href=\"\\K[^\"]+' " + tmp_html +
                        " | grep -iE '(" + arch_grep + ")' | grep '\\.AppImage$' | head -n 1";
            dl_url = Executor::shell(parse_cmd).stdout_data;
            while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();
            if (!dl_url.empty()) format_key = "AppImage";
        }

        // 阅后即焚
        CommandBuilder("rm").add_flag("-f").add_arg(tmp_html).add_raw("2>/dev/null").execute();

        if (dl_url.empty()) {
            Logger::error(_f("swcenter.wechat.not_found", arch));
            return;
        }

        if (dl_url.find("http") != 0) {
            if (dl_url.front() == '/') dl_url = "https://linux.weixin.qq.com" + dl_url;
            else dl_url = "https://linux.weixin.qq.com/" + dl_url;
        }

        Logger::info(_f("swcenter.wechat.dl_link_resolved", dl_url));
        if (!Logger::confirm(_("swcenter.wechat.confirm_dl"))) {
            return;
        }

        Logger::step(_("swcenter.wechat.downloading"));

        // 5. 提取文件名并设定独立的下载路径
        std::string filename = CommandBuilder("basename").add_arg(dl_url).execute().stdout_data;
        while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r')) filename.pop_back();

        if (filename.empty() || filename.find('?') != std::string::npos) {
            filename = "wechat_official_" + arch + "." + format_key;
        }

        // 脱离 DeveloperTools，在此处独立声明路径并创建目录
        std::string download_dir = "/tmp/tmoe-downloads";
        if (!fs::exists(download_dir)) {
            fs::create_directories(download_dir);
        }
        std::string dest = download_dir + "/" + filename;

        // 自包含下载逻辑
        std::string dl_cmd = "aria2c --console-log-level=warn --no-conf --continue=true "
                             "--allow-overwrite=true -s 5 -x 5 -k 1M "
                             "-d '" + download_dir + "' -o '" + filename + "' '" + dl_url + "'";

        if (!Executor::passthrough(dl_cmd).ok() || !fs::exists(dest) || fs::file_size(dest) == 0) {
            Logger::error(_("swcenter.wechat.dl_failed"));
            CommandBuilder("rm").add_flag("-f").add_arg(dest).add_raw("2>/dev/null").execute();
            return;
        }

        // 6. 执行原生统级部署
        Logger::step(_("swcenter.wechat.installing"));

        // 脱离 DeveloperTools 的 apps_lnk_dir_，直接使用标准系统路径
        std::string app_lnk_dir = "/usr/share/applications";

        if (format_key == "deb") {
            Executor::passthrough("sudo dpkg -i '" + dest + "' || sudo apt-get install -f -y");
        } else if (format_key == "rpm") {
            Executor::passthrough("sudo rpm -Uvh '" + dest + "' || sudo yum localinstall -y '" + dest + "'");
        } else {
            Executor::shell(
                "sudo mkdir -p /opt/wechat && sudo mv '" + dest +
                "' /opt/wechat/wechat.AppImage && sudo chmod +x /opt/wechat/wechat.AppImage");
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=WeChat\n"
                    "Comment=Tencent WeChat\n"
                    "Exec=/opt/wechat/wechat.AppImage --no-sandbox %U\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Icon=wechat\n"
                    "Categories=Network;InstantMessaging;\n";
            Executor::shell(
                "sudo sh -c 'cat > " + app_lnk_dir + "/wechat.desktop' <<'EOF'\n" + desktop_content + "EOF\n");
        }

        Logger::ok(_("swcenter.wechat.install_done"));
    }

    void SoftwareCenter::install_skype() {
        std::string arch = cfg_.arch;
        if (arch != "amd64") {
            Logger::error(_("swcenter.skype.x64_only"));
            return;
        }

        auto family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Arch) {
            PackageManager::install("skypeforlinux-stable-bin", family);
            return;
        }

        std::string dl_url;
        std::string dl_file;
        if (family == DistroFamily::RedHat) {
            dl_url = "https://repo.skype.com/latest/skypeforlinux-64.rpm";
            dl_file = "SKYPE.rpm";
        } else {
            dl_url = "https://repo.skype.com/latest/skypeforlinux-64.deb";
            dl_file = "SKYPE.deb";
        }

        Logger::info(_f("swcenter.skype.dl_link", dl_url));
        if (!Logger::confirm(_("swcenter.skype.confirm_install")))
            return;

        Logger::step(_("swcenter.skype.downloading"));
        auto dl_ret = Executor::passthrough(
            "cd /tmp && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dl_file + " '" + dl_url + "'"
        );

        if (!dl_ret.ok()) {
            Logger::error(_("swcenter.skype.dl_failed"));
            return;
        }

        if (family == DistroFamily::RedHat) {
            Executor::passthrough("cd /tmp && sudo yum install -y ./" + dl_file + "; rm -vf " + dl_file);
        } else {
            Executor::passthrough(
                "cd /tmp && apt-cache show ./" + dl_file + " 2>/dev/null; "
                "sudo apt install -y ./" + dl_file + "; rm -vf " + dl_file
            );
        }
        Logger::ok(_("swcenter.skype.install_done"));
    }

    void SoftwareCenter::install_mitalk() {
        Logger::step(_("swcenter.mitalk.fetching"));
        auto family = PackageManager::detect_distro_family();

        // 从 AUR 抓取最新 deb 链接
        auto url_result = Executor::shell(
            "curl --connect-timeout 15 --max-time 30 -sL "
            "'https://aur.archlinux.org/packages/mitalk/' 2>/dev/null | "
            "grep -oP 'https://[^\"]*\\.deb' | head -n 1"
        );
        std::string dl_url = url_result.stdout_data;
        while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r'))
            dl_url.pop_back();

        if (dl_url.empty()) {
            Logger::error(_("swcenter.mitalk.fetch_failed"));
            return;
        }

        // 非 deb 系用 AppImage
        if (family != DistroFamily::Debian && family != DistroFamily::Arch) {
            // 替换 .deb 为 .AppImage
            size_t pos = dl_url.rfind(".deb");
            if (pos != std::string::npos)
                dl_url.replace(pos, 4, ".AppImage");
        }

        Logger::info(_f("swcenter.mitalk.dl_link", dl_url));
        if (!Logger::confirm(_("swcenter.mitalk.confirm_install")))
            return;

        std::string dl_file = "mitalk.deb";
        Executor::passthrough(
            "cd /tmp && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dl_file + " '" + dl_url + "'"
        );

        if (family == DistroFamily::Debian || family == DistroFamily::Arch) {
            Executor::passthrough(
                "cd /tmp && sudo dpkg -i ./" + dl_file + " || sudo apt install -y ./" + dl_file +
                " 2>/dev/null || true; "
                "rm -vf " + dl_file
            );
        }
        Logger::ok(_("swcenter.mitalk.install_done"));
    }


    // ═══════════════════════════════════════════════════════════════
    // 7. 📚 Documents → office_cb_ 重定向
    // 8. 🏤 Debian Opt Repo — 对应旧 Bash explore_debian_opt_repo
    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    // 9. 🎁 Download → download_cb_ 重定向
    // 10. 🔯 Packages & System — 对应旧 Bash tmoe_software_package_menu
    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    // 11. 🥙 Zsh → zsh_cb_ 重定向
    // 12. 🥗 File Shared — 对应旧 Bash personal_netdisk
    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    // 13. 💔 Remove — 对应旧 Bash tmoe_other_options_menu
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::remove_gui() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        Logger::step(_("swcenter.cleanup.remove_gui_step"));
        auto confirm = Executor::passthrough(cfg_.tui_bin +
                                             " --yesno \"" + _("swcenter.cleanup.remove_gui_confirm") + "\" 0 0");
        if (confirm.exit_code != 0) return;

        switch (family) {
            case DistroFamily::Debian: {
                // 每个包名独立移除，一个失败不影响其他
                for (const auto &pkg: {
                         // xfce
                         "xfce4", "xfce4-goodies", "xfce4-terminal", "xfce4-panel", "thunar",
                         "xfce4-whiskermenu-plugin", "xfce4-taskmanager",
                         "xfce4-places-plugin", "xfce4-netload-plugin", "xfce4-battery-plugin",
                         "xfce4-datetime-plugin", "xfce4-verve-plugin", "xfce4-mount-plugin",
                         "xfce4-screenshooter", "xfce4-clipman-plugin", "xfce4-pulseaudio-plugin",
                         "xfce4-panel-profiles", "xfpanel-switch",
                         "thunar-archive-plugin", "engrampa", "ristretto", "mousepad", "menulibre",
                         "qt5ct", "mugshot", "xfwm4-theme-breeze",
                         "gvfs", "gvfs-backends", "gvfs-fuse",
                         // lxde
                         "lxde-core", "lxterminal", "lxde", "lxde-common",
                         // lxqt
                         "lxqt-core", "lxqt", "lxqt-qtplugin", "qterminal", "qterminal-l10n",
                         "pcmanfm-qt", "pcmanfm-qt-l10n", "openbox", "lxqt-session", "lxqt-config",
                         "lxqt-theme-debian", "lxqt-session-l10n",
                         // mate
                         "mate-desktop-environment", "mate-desktop-environment-core", "mate-terminal",
                         "mate-session-manager", "mate-settings-daemon", "marco", "mate-panel",
                         // kde
                         "kde-plasma-desktop", "plasma-desktop", "kde-full", "kde-standard",
                         "kubuntu-desktop",
                         // gnome
                         "gnome-session", "gnome-shell", "gnome-core",
                         // cinnamon
                         "cinnamon-desktop-environment", "cinnamon", "cinnamon-l10n",
                         // budgie
                         "budgie-desktop", "budgie-core",
                         // dde/deepin
                         "ubuntudde-dde", "ubuntudde-dde-extras", "deepin-terminal",
                         "deepin-desktop-environment",
                         // ukui
                         "ukui-session-manager", "ukui-menu", "ukui-control-center",
                         "ukui-screensaver", "ukui-themes", "peony", "ubuntukylin-desktop",
                         // cutefish
                         "cutefish", "cutefish-core", "cutefish-settings",
                         "cutefish-dock", "cutefish-launcher", "cutefish-filemanager",
                         "cutefish-terminal", "cutefish-texteditor",
                         // wm
                         "icewm", "openbox", "fvwm", "awesome", "enlightenment", "fluxbox",
                         "i3", "i3-wm", "xmonad", "metacity", "twm", "dwm",
                         // common
                         "tightvncserver", "tigervnc-standalone-server", "dbus-x11",
                         "fonts-noto-cjk", "fonts-noto-color-emoji",
                     }) {
                    PackageManager::remove(std::string(pkg), family);
                }
                Executor::passthrough(
                    "sudo apt autoremove --purge -y 2>/dev/null || sudo apt autoremove -y 2>/dev/null || true");
                break;
            }
            case DistroFamily::Arch:
                for (const auto &pkg: {
                         "xfce4", "xfce4-goodies", "xfce4-terminal",
                         "mate", "mate-extra",
                         "lxde", "lxqt", "plasma-desktop",
                         "gnome", "gnome-extra", "gnome-tweaks",
                         "cinnamon", "cinnamon-translations",
                         "deepin", "deepin-extra", "deepin-kwin",
                         "ukui", "cutefish",
                         "i3-wm", "i3status", "i3lock", "dmenu",
                         "xmonad", "xmobar", "openbox", "fluxbox",
                         "awesome", "enlightenment", "icewm", "twm", "dwm",
                     })
                    PackageManager::remove(std::string(pkg), family);
                break;
            case DistroFamily::RedHat:
                for (const auto &grp: {
                         "xfce", "mate-desktop", "lxde-desktop", "lxqt",
                         "KDE", "GNOME", "Cinnamon Desktop"
                     })
                    Executor::passthrough("sudo dnf groupremove -y \"" + std::string(grp) + "\" 2>/dev/null || true");
                PackageManager::remove("deepin-desktop", family);
                break;
            default:
                for (const auto &pkg: {
                         "xfce4", "xfce4-goodies", "lxde", "lxqt", "mate-desktop",
                         "kde-plasma-desktop", "gnome-session", "gnome-shell", "cinnamon",
                         "budgie-desktop", "ukui-session-manager", "cutefish",
                         "deepin-desktop-environment",
                     })
                    PackageManager::remove(std::string(pkg), family);
                break;
        }

        // ── 共享清理：桌面配置残余（所有发行版通用）──
        const char *home = std::getenv("HOME");
        std::string h = home ? home : "/root";
        for (const auto *dir: {
                 "/.config/xfce4", "/.config/mate", "/.config/lxde",
                 "/.config/lxqt", "/.config/kde", "/.config/plasma",
                 "/.config/kde.org", "/.config/kdeconnect",
                 "/.config/dconf", "/.config/gnome", "/.config/cinnamon",
                 "/.config/budgie", "/.config/deepin", "/.config/ukui",
                 "/.config/cutefish", "/.config/KDE", "/.config/session",
                 "/.kde", "/.local/share/kde", "/.local/share/plasma",
                 "/.cache/xfce4", "/.cache/mate", "/.cache/lxde", "/.cache/lxqt",
                 "/.cache/kde", "/.cache/plasma"
             })
            Executor::passthrough("rm -rf " + h + dir + " 2>/dev/null || true");
        Executor::passthrough("rm -rf ~/.vnc ~/.dbus ~/.cache/sessions 2>/dev/null || true");
        Executor::passthrough("sudo rm -rf /etc/tigervnc 2>/dev/null || true");
        // ~/.local/share 各桌面数据
        for (const auto *dir: {
                 "/.local/share/xfce4", "/.local/share/kde", "/.local/share/plasma",
                 "/.local/share/mate", "/.local/share/gnome", "/.local/share/cinnamon",
                 "/.local/share/budgie", "/.local/share/deepin", "/.local/share/ukui",
                 "/.local/share/lxde", "/.local/share/lxqt", "/.local/share/cutefish"
             })
            Executor::passthrough("rm -rf " + h + dir + " 2>/dev/null || true");
        for (const auto *script: {
                 "/usr/local/bin/startvnc", "/usr/local/bin/stopvnc",
                 "/usr/local/bin/startxsdl", "/usr/local/bin/novnc",
                 "/usr/local/bin/startx11vnc", "/usr/local/bin/x11vncpasswd",
                 "/usr/local/bin/tightvnc", "/usr/local/bin/tigervnc",
                 "/usr/local/bin/gnome-shell-x11", "/usr/local/bin/budgie-desktop-builtin"
             })
            Executor::passthrough(std::string("sudo rm -f ") + script + " 2>/dev/null || true");
        Executor::passthrough("sudo rm -rf /etc/X11/xinit 2>/dev/null || true");
    }

    void SoftwareCenter::remove_browser() {
        // 对应旧 Bash: 火狐娘 vs chromium娘 二选一
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string confirm = cfg_.tui_bin +
                              " --title \"" + _("swcenter.cleanup.browser_title") +
                              "\" --yes-button \"Firefox\" --no-button \"chromium\""
                              " --yesno '" + _("swcenter.cleanup.browser_dialog_text") + "' 10 60";
        auto choice = Executor::passthrough(confirm);
        // exit_code: 0=Firefox(yes), 1=chromium(no), 255=ESC

        if (choice.exit_code == 0) {
            auto f_confirm = Executor::passthrough(cfg_.tui_bin +
                                                   " --yesno \"" + _("swcenter.cleanup.browser_firefox_confirm") +
                                                   "\" 0 0");
            if (f_confirm.exit_code != 0) return;
            for (const auto *pkg: {
                     "firefox-esr", "firefox-esr-l10n-zh-cn",
                     "firefox", "firefox-l10n-zh-cn", "firefox-locale-zh-hans"
                 })
                PackageManager::remove(std::string(pkg), family);
        } else if (choice.exit_code == 1) {
            auto c_confirm = Executor::passthrough(cfg_.tui_bin +
                                                   " --yesno \"" + _("swcenter.cleanup.browser_chromium_confirm") +
                                                   "\" 0 0");
            if (c_confirm.exit_code != 0) return;
            Executor::passthrough(
                "apt-mark unhold chromium-browser chromium-browser-l10n chromium-codecs-ffmpeg-extra 2>/dev/null || true");
            for (const auto *pkg: {
                     "chromium", "chromium-l10n",
                     "chromium-browser", "chromium-browser-l10n"
                 })
                PackageManager::remove(std::string(pkg), family);
        }
        Executor::passthrough("sudo apt autoremove --purge -y 2>/dev/null || true");
    }

    void SoftwareCenter::remove_tmoe_tools() {
        Logger::step(_("swcenter.cleanup.remove_tmoe_step"));
        Logger::warn(_("swcenter.cleanup.remove_tmoe_warn"));

        auto tm_confirm = Executor::passthrough(cfg_.tui_bin +
                                                " --yesno \"" + _("swcenter.cleanup.remove_tmoe_confirm") + "\" 0 0");
        if (tm_confirm.exit_code != 0) return;

        CommandBuilder("sudo").add_arg("rm")
                .add_flag("-rfv")
                .add_arg("/usr/local/bin/tmoe")
                .add_arg("/usr/local/bin/tmoes")
                .add_arg("/usr/local/bin/tome")
                .add_arg("/usr/local/bin/startvnc")
                .add_arg("/usr/local/bin/stopvnc")
                .add_arg("/usr/local/bin/novnc")
                .add_arg("/usr/local/bin/startx11vnc")
                .add_arg("/usr/local/bin/startxsdl")
                .add_arg("/usr/local/bin/x11vncpasswd")
                .add_arg("/usr/local/bin/debian")
                .add_arg("/usr/local/bin/debian-i")
                .add_arg("/usr/local/share/tmoe-linux")
                .add_arg("~/.config/tmoe-linux")
                .add_raw("2>/dev/null || true")
                .execute();

        // 移除依赖包
        auto family = infer_family_from_config(cfg_.linux_distro);
        for (const auto *pkg: {"git", "aria2", "pv", "wget", "curl", "less", "xz-utils", "newt", "whiptail"})
            PackageManager::remove(std::string(pkg), family);

        Logger::warn(_("swcenter.cleanup.remove_tmoe_done"));
    }
} // namespace tmoe::domain
