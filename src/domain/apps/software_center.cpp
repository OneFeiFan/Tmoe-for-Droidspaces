#include "domain/apps/software_center.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/command_builder.hpp"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    SoftwareCenter::SoftwareCenter(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void SoftwareCenter::run_software_center_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.menu_title")
                               + "\" --menu \"" + _("swcenter.menu_prompt") + "\" 0 0 0 "
                               "\"1\"  \"" + _("swcenter.browser") + "\" "
                               "\"2\"  \"" + _("swcenter.dev") + "\" "
                               "\"3\"  \"" + _("swcenter.electron") + "\" "
                               "\"4\"  \"" + _("swcenter.media") + "\" "
                               "\"5\"  \"" + _("swcenter.sns") + "\" "
                               "\"6\"  \"" + _("swcenter.games") + "\" "
                               "\"7\"  \"" + _("swcenter.docs") + "\" "
                               "\"8\"  \"" + _("swcenter.debian_opt") + "\" "
                               "\"9\"  \"" + _("swcenter.download") + "\" "
                               "\"10\" \"" + _("swcenter.pkg_gui") + "\" "
                               "\"11\" \"" + _("swcenter.zsh") + "\" "
                               "\"12\" \"" + _("swcenter.fileshare") + "\" "
                               "\"13\" \"" + _("swcenter.cleanup") + "\" "
                               "\"0\"  \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1" && browser_cb_) browser_cb_();
            else if (ch == "2" && dev_cb_) dev_cb_();
            else if (ch == "3") run_electron_apps_menu();
            else if (ch == "4") run_media_menu();
            else if (ch == "5") run_sns_menu();
            else if (ch == "6") run_games_menu();
            else if (ch == "7" && office_cb_) office_cb_();
            else if (ch == "8") run_debian_opt_menu();
            else if (ch == "9" && download_cb_) download_cb_();
            else if (ch == "10") run_pkg_gui_menu();
            else if (ch == "11" && zsh_cb_) zsh_cb_();
            else if (ch == "12") run_fileshare_menu();
            else if (ch == "13") run_cleanup_menu();
            // 各子菜单/callback 内部自行 handle press_enter
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 3. ⚛️ Electron Apps — 对应旧 Bash tmoe_electron_repo → sources/electron-apps
    //    所有应用从 https://packages.tmoe.me/apps/<name>/app.tar.xz 下载
    // ═══════════════════════════════════════════════════════════════

    void SoftwareCenter::run_electron_apps_menu() {
        while (true) {
            if (!fs::exists("/opt/electron/electron")) {
                Logger::info("本electron仓库由2moe进行维护，感谢各个项目的原开发者。");
                Logger::info("要求:gnu libc, 支持arch/manjaro,debian/ubuntu,fedora,opensuse,void等");
                Logger::info("暂不支持alpine(musl libc)");
                Logger::press_enter();
            }

            std::string menu = cfg_.tui_bin + " --title \"ELECTRON APPS\""
                               " --menu \"electron:Build cross-platform desktop apps with JavaScript,HTML,and CSS\" 0 0 0 "
                               "\"1\" \"🎵 music:以雅以南,以龠不僭\" "
                               "\"2\" \"📺 videos视频:全网影视搜索,无损切割视频\" "
                               "\"3\" \"📝 notes笔记:记录灵感,撰写文档,整理材料,回顾日记\" "
                               "\"4\" \"🍎 virtual machine虚拟机:win95,macos8\" "
                               "\"5\" \"👾 development程序开发:神经网络,深度学习\" "
                               "\"6\" \"⚛️ electron manager\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") run_electron_music_menu();
            else if (ch == "2") run_electron_video_menu();
            else if (ch == "3") run_electron_note_menu();
            else if (ch == "4") run_electron_vm_menu();
            else if (ch == "5") run_electron_dev_menu();
            else if (ch == "6") run_electron_manager();
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════
    // 通用: 安装或卸载子菜单
    // ═══════════════════════════════════
    void SoftwareCenter::electron_install_or_remove(const std::string &app_name) {
        std::string menu = cfg_.tui_bin + " --title \"" + app_name + " manager\""
                           " --menu \"您要对 " + app_name + " 小可爱做什么?\" 0 0 0 "
                           "\"1\" \"install/upgrade 安装/更新\" "
                           "\"2\" \"remove 卸载\" "
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

        Logger::step("安装 Electron 运行时...");
        CommandBuilder("mkdir").add_flag("-pv").add_arg("/opt").execute();

        // 下载最新 Electron
        auto result = Executor::passthrough(
            "cd /tmp && "
            "ELECTRON_VER=18.2.3; "
            "ELECTRON_URL=\"https://npmmirror.com/mirrors/electron/v${ELECTRON_VER}/electron-v${ELECTRON_VER}-linux-x64.zip\"; "
            "rm -rf .electron_tmp 2>/dev/null; mkdir -pv .electron_tmp; cd .electron_tmp; "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 3 -x 3 -k 1M -o electron.zip \"${ELECTRON_URL}\" && "
            "unzip -qo electron.zip -d /opt/electron && "
            "chmod a+rx /opt/electron/electron && "
            "ln -sf /opt/electron/electron /usr/bin/electron"
        );

        if (!result.ok() || !fs::exists("/opt/electron/electron")) {
            Logger::warn("Electron 运行时安装失败，部分应用可能无法启动");
        } else {
            Logger::ok("Electron 运行时已安装到 /opt/electron");
        }
        CommandBuilder("rm").add_flag("-rf").add_arg("/tmp/.electron_tmp").add_raw("2>/dev/null").execute();
    }

    // ═══════════════════════════════════
    // 下载 tmoe electron app
    // ═══════════════════════════════════
    void SoftwareCenter::download_tmoe_electron_app(const std::string &app_name) {
        const std::string pkg_url = "https://packages.tmoe.me";
        std::string tmp_dir = "/tmp/." + app_name + "_TEMP_FOLDER";

        Executor::shell("rm -rf " + tmp_dir + " 2>/dev/null; mkdir -pv " + tmp_dir);
        CommandBuilder("mkdir").add_flag("-pv").add_arg("/opt").execute();

        Logger::step("正在下载 " + app_name + " ...");
        auto dl_ret = Executor::passthrough(
            "cd " + tmp_dir + " && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -x 3 -k 1M --split 3 -o app.tar.xz "
            "'" + pkg_url + "/apps/" + app_name + "/app.tar.xz'"
        );

        if (!dl_ret.ok()) {
            Logger::error("下载 " + app_name + " 失败");
            CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
            return;
        }

        Logger::step("正在解压 " + app_name + " ...");
        Executor::passthrough("cd " + tmp_dir + " && tar -Jxvf app.tar.xz -C /opt");

        if (!fs::exists("/opt/" + app_name)) {
            Logger::warn(app_name + " 解压后未找到，重试中...");
            // 重试一次
            Executor::passthrough(
                "cd " + tmp_dir + " && "
                "aria2c --console-log-level=warn --no-conf --continue=true "
                "--allow-overwrite=true -x 3 -k 1M --split 3 -o app.tar.xz "
                "'" + pkg_url + "/apps/" + app_name + "/app.tar.xz' && "
                "tar -Jxvf app.tar.xz -C /opt"
            );
        }

        if (!fs::exists("/opt/" + app_name)) {
            Logger::error("安装 " + app_name + " 失败，请检查网络或手动安装");
            CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
            return;
        }

        // 修复权限并复制 .desktop 文件
        Executor::shell(
            "chmod -Rv 755 /opt/" + app_name + "/usr/bin/ 2>/dev/null || true; "
            "find /opt/" + app_name + " -type d -print | xargs chmod -v a+rx 2>/dev/null || true; "
            "find /opt/" + app_name + " -type f -print | xargs chmod a+r 2>/dev/null || true"
        );

        // 复制 .desktop 到系统
        Executor::shell(
            "cd /opt/" + app_name + " && "
            "if [ -e ." + apps_lnk_dir_ + "/" + app_name + ".desktop ]; then "
            "  cp -vf ." + apps_lnk_dir_ + "/" + app_name + ".desktop " + apps_lnk_dir_ + "/; "
            "fi"
        );

        CommandBuilder("rm").add_flag("-rf").add_arg(tmp_dir).add_raw("2>/dev/null").execute();
        Logger::ok(app_name + " 安装完成！请从应用菜单或命令行启动。");
    }

    void SoftwareCenter::remove_electron_app(const std::string &app_name) {
        Logger::warn("即将卸载 " + app_name + ":");
        Logger::warn("  rm -rv /opt/" + app_name + " " + apps_lnk_dir_ + "/" + app_name + ".desktop");
        if (!Logger::confirm("确认卸载 " + app_name + " ?"))
            return;
        CommandBuilder("rm").add_flag("-rf")
            .add_arg("/opt/" + app_name)
            .add_arg(apps_lnk_dir_ + "/" + app_name + ".desktop")
            .add_raw("2>/dev/null").execute();
        Logger::ok(app_name + " 已卸载");
    }

    // ═══════════════════════════════════
    // 各分类子菜单
    // ═══════════════════════════════════
    void SoftwareCenter::run_electron_music_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"Music\" --menu \"Music\" 0 0 0 "
                               "\"1\" \"electron-netease-cloud-music (云音乐)\" "
                               "\"2\" \"YesPlayMusic (高颜值的第三方网易云播放器)\" "
                               "\"3\" \"LISTEN1 (免费音乐聚合)\" "
                               "\"4\" \"petal (第三方豆瓣FM客户端)\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") electron_install_or_remove("electron-netease-cloud-music");
            else if (ch == "2") electron_install_or_remove("yesplaymusic");
            else if (ch == "3") electron_install_or_remove("listen1");
            else if (ch == "4") electron_install_or_remove("petal");
            Logger::press_enter();
        }
    }

    void SoftwareCenter::run_electron_video_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"VIDEO APP\" --menu \"影視\" 0 0 0 "
                               "\"1\" \"📺 bilibili 哔哩哔哩PC客户端\" "
                               "\"2\" \"zy-player (搜索全网影视)\" "
                               "\"3\" \"🎬 腾讯视频 (Linux在线视频软件)\" "
                               "\"4\" \"lossless-cut (无损剪切音视频工具)\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") electron_install_or_remove("bilibili");
            else if (ch == "2") electron_install_or_remove("zy-player");
            else if (ch == "3") electron_install_or_remove("tenvideo_universal");
            else if (ch == "4") {
                // lossless-cut 需要 ffmpeg
                auto family = infer_family_from_config(cfg_.linux_distro);
                PackageManager::install("ffmpeg", family);
                electron_install_or_remove("lossless-cut");
            }
            Logger::press_enter();
        }
    }

    void SoftwareCenter::run_electron_note_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"NOTE APP\" --menu \"筆記\" 0 0 0 "
                               "\"1\" \"obsidian (a wonderful markdown app)\" "
                               "\"2\" \"gridea (静态博客写作app)\" "
                               "\"3\" \"draw.io (思维导图绘制工具)\" "
                               "\"4\" \"simplenote (轻量级开源跨平台云笔记)\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") electron_install_or_remove("obsidian");
            else if (ch == "2") electron_install_or_remove("gridea");
            else if (ch == "3") {
                auto family = infer_family_from_config(cfg_.linux_distro);
                PackageManager::install({"libindicator3-7", "libappindicator3-1"}, family);
                electron_install_or_remove("draw.io");
            } else if (ch == "4") electron_install_or_remove("simplenote");
            Logger::press_enter();
        }
    }

    void SoftwareCenter::run_electron_vm_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"VIRTUAL MACHINE APP\""
                               " --menu \"Javascript vm is not qemu\" 0 0 0 "
                               "\"1\" \"MacOS8 (上古时期苹果Macintosh系统)\" "
                               "\"2\" \"Win95 (微软windows操作系统)\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") {
                Logger::info("下载大小约131.09MiB,解压后约占658M");
                if (!Logger::confirm("确认安装 macintosh.js ?")) continue;
                ensure_electron_runtime();
                Executor::passthrough(
                    "cd /tmp && "
                    "TEMP_FOLDER='.macintosh.js_TEMP_FOLDER'; "
                    "rm -rf ${TEMP_FOLDER} 2>/dev/null; "
                    "git clone --depth=1 https://gitee.com/ak2/electron_macos8.git ${TEMP_FOLDER} && "
                    "cd ${TEMP_FOLDER} && "
                    "cat .vm_* >vm.tar.xz && "
                    "tar -PpJxvf vm.tar.xz && "
                    "cd .. && rm -rf ${TEMP_FOLDER}"
                );
                Logger::ok("MacOS8 安装完成");
            } else if (ch == "2") {
                Logger::info("下载大小约166.19MiB,解压后约占1.2G");
                if (!Logger::confirm("确认安装 windows95 ?")) continue;
                ensure_electron_runtime();
                Executor::passthrough(
                    "cd /tmp && "
                    "TEMP_FOLDER='.windows95_TEMP_FOLDER'; "
                    "rm -rf ${TEMP_FOLDER} 2>/dev/null; "
                    "git clone --depth=1 https://gitee.com/ak2/electron_win95.git ${TEMP_FOLDER} && "
                    "cd ${TEMP_FOLDER} && "
                    "cat .vm_* >vm.tar.xz && "
                    "tar -PpJxvf vm.tar.xz && "
                    "cd .. && rm -rf ${TEMP_FOLDER}"
                );
                Logger::ok("Windows 95 安装完成");
            }
            Logger::press_enter();
        }
    }

    void SoftwareCenter::run_electron_dev_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"DEVELOPMENT\""
                               " --menu \"Which software do you want to install?\" 0 0 0 "
                               "\"1\" \"netron (神经网络,深度学习和机器学习模型的可视化工具)\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") electron_install_or_remove("netron");
            Logger::press_enter();
        }
    }

    void SoftwareCenter::run_electron_manager() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"electron manager\""
                               " --menu \"您要对electron小可爱做什么?\" 0 0 0 "
                               "\"1\" \"install/upgrade 安装/更新 electron\" "
                               "\"2\" \"remove electron (稳定版)\" "
                               "\"3\" \"remove electron-v8.x\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") {
                ensure_electron_runtime();
            } else if (ch == "2") {
                Logger::warn("卸载后将导致依赖electron的应用无法正常运行。");
                Logger::warn("  rm -rvf /opt/electron /usr/bin/electron");
                if (Logger::confirm("确认卸载 electron?")) {
                    CommandBuilder("rm").add_flag("-rf")
                        .add_arg("/opt/electron")
                        .add_arg("/usr/bin/electron")
                        .add_raw("2>/dev/null").execute();
                    Logger::ok("electron 已卸载");
                }
            } else if (ch == "3") {
                Logger::warn("部分软件依赖于旧版electron,卸载后将导致这些软件无法正常运行。");
                Logger::warn("  rm -rvf /opt/electron-v8");
                if (Logger::confirm("确认卸载 electron-v8?")) {
                    CommandBuilder("rm").add_flag("-rf").add_arg("/opt/electron-v8").add_raw("2>/dev/null").execute();
                    Logger::ok("electron-v8 已卸载");
                }
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 4. 🎶 Multimedia — 对应旧 Bash tmoe_multimedia_menu (12项)
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_media_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.media_menu")
                           + "\" --menu \"Which software do you want to install?\" 0 0 0 "
                           "\"1\"  \"🗜️ " + _("swcenter.batch_compress") + "\" "
                           "\"2\"  \"📽️ " + _("swcenter.mpv") + "\" "
                           "\"3\"  \"🎥 " + _("swcenter.smplayer") + "\" "
                           "\"4\"  \"🇵 " + _("swcenter.peek") + "\" "
                           "\"5\"  \"🖼 " + _("swcenter.gimp") + "\" "
                           "\"6\"  \"" + _("swcenter.kolourpaint") + "\" "
                           "\"7\"  \"🍊 " + _("swcenter.clementine") + "\" "
                           "\"8\"  \"🎞️ " + _("swcenter.parole") + "\" "
                           "\"9\"  \"🎧 " + _("swcenter.netease") + "\" "
                           "\"10\" \"🎼 " + _("swcenter.audacity") + "\" "
                           "\"11\" \"🎶 " + _("swcenter.ardour") + "\" "
                           "\"12\" \"" + _("swcenter.spotify") + "\" "
                           "\"0\"  \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") {
            Logger::step("batch-compress");
            PackageManager::install("imagemagick", family);
        } else if (ch == "2") PackageManager::install("mpv", family);
        else if (ch == "3") PackageManager::install("smplayer", family);
        else if (ch == "4") PackageManager::install("peek", family);
        else if (ch == "5") PackageManager::install("gimp", family);
        else if (ch == "6") PackageManager::install("kolourpaint", family);
        else if (ch == "7") PackageManager::install("clementine", family);
        else if (ch == "8") PackageManager::install("parole", family);
        else if (ch == "9") PackageManager::install("netease-cloud-music", family);
        else if (ch == "10") PackageManager::install("audacity", family);
        else if (ch == "11") PackageManager::install("ardour", family);
        else if (ch == "12") PackageManager::install("spotify-client", family);
        Logger::press_enter();
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
        Logger::step("正在通过腾讯官方 API 获取 LinuxQQ 最新版本...");
        Logger::info("若安装失败，请前往官网手动下载安装: https://im.qq.com/linuxqq/download.html");

        auto family = PackageManager::detect_distro_family();

        // 1. Arch Linux 专属特权：直接走官方包管理器 (AUR) 最为优雅稳定
        if (family == DistroFamily::Arch) {
            Logger::info("检测到 Arch Linux，将通过包管理器直接安装...");
            PackageManager::install("linuxqq", family);
            Logger::ok("LinuxQQ 安装流程已移交包管理器");
            return;
        }

        // 2. 安全落地配置文件：使用官方动态分发接口
        std::string tmp_json = "/tmp/qq_pc_config.json";
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        Executor::shell(
            "curl " + std::string(curl_opts) +
            " 'https://cdn-go.cn/qq-web/im.qq.com_new/latest/rainbow/pcConfig.json' -o " + tmp_json);

        if (Executor::shell("grep '\"Linux\"' " + tmp_json + " > /dev/null 2>&1").exit_code != 0) {
            Logger::error("获取 QQ 官方配置失败，请检查网络或防火墙。");
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();
            return;
        }

        // 3. 提取版本号
        std::string version = Executor::shell("jq -r '.Linux.version' " + tmp_json).stdout_data;
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();

        if (version.empty()) version = "未知版本";

        if (!Logger::confirm("确认安装 LinuxQQ " + version + " ?")) {
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
            Logger::error("当前系统架构 (" + arch + ") 暂不支持官方 Linux QQ");
            CommandBuilder("rm").add_flag("-f").add_arg(tmp_json).add_raw("2>/dev/null").execute();
            return;
        }

        std::string dl_url = Executor::shell(jq_cmd).stdout_data;
        while (!dl_url.empty() && (dl_url.back() == '\n' || dl_url.back() == '\r')) dl_url.pop_back();

        // 柔性降级策略：如果当前架构缺失指定的 deb/rpm，自动降级为全系统通用的 AppImage
        if (dl_url.empty() || dl_url == "null") {
            Logger::warn("官方尚未提供当前环境的 " + format_key + " 原生包，尝试降级为通用 AppImage...");
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
            Logger::error("获取官方下载直链失败！官方可能调整了 API。");
            return;
        }

        Logger::info("直链解析成功: " + dl_url);
        Logger::step("正在下载安装包...");

        // 5. 下载包体 (使用独立 -o 避免路径拼接陷阱)
        std::string dest_file = "LINUXQQ" + ext;
        std::string download_cmd = "cd /tmp && aria2c --console-log-level=warn --no-conf --continue=true "
                                   "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dest_file + " '" + dl_url + "'";

        if (!Executor::passthrough(download_cmd).ok()) {
            Logger::error("下载失败，请检查网络环境。");
            return;
        }

        // 6. 执行系统级原生安装
        Logger::step("正在执行安装部署...");
        if (ext == ".deb") {
            Executor::passthrough(
                "cd /tmp && apt-cache show ./" + dest_file + " 2>/dev/null; apt install -y ./" + dest_file +
                " && rm -vf " + dest_file);
        } else if (ext == ".rpm") {
            Executor::passthrough(
                "cd /tmp && rpm -Uvh ./" + dest_file + " || yum localinstall -y ./" + dest_file + " && rm -vf " +
                dest_file);
        } else {
            // AppImage 配置部署
            Executor::shell(
                "mkdir -p /opt/linuxqq && mv /tmp/" + dest_file +
                " /opt/linuxqq/linuxqq.AppImage && chmod +x /opt/linuxqq/linuxqq.AppImage");
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=QQ\n"
                    "Comment=Tencent QQ\n"
                    "Exec=/opt/linuxqq/linuxqq.AppImage --no-sandbox %U\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Icon=qq\n"
                    "Categories=Network;InstantMessaging;\n";
            Executor::shell("cat > /usr/share/applications/linuxqq.desktop <<'EOF'\n" + desktop_content + "EOF\n");
        }

        Logger::ok("LinuxQQ " + version + " 安装完成！");
    }

    void SoftwareCenter::install_wechat() {
        Logger::step("正在通过微信官方网站获取最新原生版本...");
        Logger::info("数据源: https://linux.weixin.qq.com/");

        auto family = PackageManager::detect_distro_family();

        // 1. Arch Linux 专属特权：AUR 的 linux-weixin 是由社区维护的官方重打包版本
        if (family == DistroFamily::Arch) {
            Logger::info("检测到 Arch Linux，将通过包管理器直接安装官方原生微信...");
            PackageManager::install("linux-weixin", family);
            Logger::ok("微信安装流程已移交包管理器");
            return;
        }

        // 2. 安全策略：获取官网 HTML 源码落地为临时文件
        std::string tmp_html = "/tmp/wechat_linux.html";
        const char *curl_opts = "-4 --retry 3 --retry-delay 2 --connect-timeout 10 --max-time 30 -sL "
                "-A 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'";

        Executor::shell("curl " + std::string(curl_opts) + " 'https://linux.weixin.qq.com/' -o " + tmp_html);

        if (!fs::exists(tmp_html) || fs::file_size(tmp_html) == 0) {
            Logger::error("获取微信官网页面失败，请检查网络连接。");
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
            Logger::error("当前系统架构 (" + arch + ") 暂不支持官方 Linux 微信");
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
            Logger::error("在官网未找到架构 " + arch + " 的原生安装包。");
            return;
        }

        if (dl_url.find("http") != 0) {
            if (dl_url.front() == '/') dl_url = "https://linux.weixin.qq.com" + dl_url;
            else dl_url = "https://linux.weixin.qq.com/" + dl_url;
        }

        Logger::info("解析到官方原生下载直链: " + dl_url);
        if (!Logger::confirm("确认下载并安装官方原生版 Linux 微信吗？")) {
            return;
        }

        Logger::step("正在下载原生安装包...");

        // 5. 提取文件名并设定独立的下载路径
        std::string filename = Executor::shell("basename '" + dl_url + "'").stdout_data;
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
            Logger::error("官方微信下载失败，请检查网络环境。");
            CommandBuilder("rm").add_flag("-f").add_arg(dest).add_raw("2>/dev/null").execute();
            return;
        }

        // 6. 执行原生统级部署
        Logger::step("正在部署官方原生版微信...");

        // 脱离 DeveloperTools 的 apps_lnk_dir_，直接使用标准系统路径
        std::string app_lnk_dir = "/usr/share/applications";

        if (format_key == "deb") {
            Executor::passthrough("sudo dpkg -i '" + dest + "' || sudo apt-get install -f -y");
        } else if (format_key == "rpm") {
            Executor::passthrough("sudo rpm -Uvh '" + dest + "' || sudo yum localinstall -y '" + dest + "'");
        } else {
            Executor::shell(
                "mkdir -p /opt/wechat && mv '" + dest +
                "' /opt/wechat/wechat.AppImage && chmod +x /opt/wechat/wechat.AppImage");
            std::string desktop_content =
                    "[Desktop Entry]\n"
                    "Name=WeChat\n"
                    "Comment=Tencent WeChat\n"
                    "Exec=/opt/wechat/wechat.AppImage --no-sandbox %U\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Icon=wechat\n"
                    "Categories=Network;InstantMessaging;\n";
            Executor::shell("cat > " + app_lnk_dir + "/wechat.desktop <<'EOF'\n" + desktop_content + "EOF\n");
        }

        Logger::ok("官方原生版 Linux WeChat 安装完成！");
    }

    void SoftwareCenter::install_skype() {
        std::string arch = cfg_.arch;
        if (arch != "amd64") {
            Logger::error("Skype 仅支持 x86_64 架构");
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

        Logger::info("下载链接: " + dl_url);
        if (!Logger::confirm("确认安装 Skype ?"))
            return;

        Logger::step("正在下载 Skype ...");
        auto dl_ret = Executor::passthrough(
            "cd /tmp && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dl_file + " '" + dl_url + "'"
        );

        if (!dl_ret.ok()) {
            Logger::error("Skype 下载失败");
            return;
        }

        if (family == DistroFamily::RedHat) {
            Executor::passthrough("cd /tmp && yum install -y ./" + dl_file + "; rm -vf " + dl_file);
        } else {
            Executor::passthrough(
                "cd /tmp && apt-cache show ./" + dl_file + " 2>/dev/null; "
                "apt install -y ./" + dl_file + "; rm -vf " + dl_file
            );
        }
        Logger::ok("Skype 安装完成");
    }

    void SoftwareCenter::install_mitalk() {
        Logger::step("正在获取米聊下载信息...");
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
            Logger::error("无法获取米聊下载链接");
            return;
        }

        // 非 deb 系用 AppImage
        if (family != DistroFamily::Debian && family != DistroFamily::Arch) {
            // 替换 .deb 为 .AppImage
            size_t pos = dl_url.rfind(".deb");
            if (pos != std::string::npos)
                dl_url.replace(pos, 4, ".AppImage");
        }

        Logger::info("下载链接: " + dl_url);
        if (!Logger::confirm("确认安装 米聊 ?"))
            return;

        std::string dl_file = "mitalk.deb";
        Executor::passthrough(
            "cd /tmp && "
            "aria2c --console-log-level=warn --no-conf --continue=true "
            "--allow-overwrite=true -s 5 -x 5 -k 1M -o " + dl_file + " '" + dl_url + "'"
        );

        if (family == DistroFamily::Debian || family == DistroFamily::Arch) {
            Executor::passthrough(
                "cd /tmp && dpkg -i ./" + dl_file + " || apt install -y ./" + dl_file + " 2>/dev/null || true; "
                "rm -vf " + dl_file
            );
        }
        Logger::ok("米聊安装完成");
    }

    void SoftwareCenter::run_sns_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);
            std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.sns_menu")
                               + "\" --menu \"Which software do you want to install?\" 0 0 0 "
                               "\"1\"  \"LinuxQQ (腾讯开发的IM软件,从心出发,趣无止境)\" "
                               "\"2\"  \"Wechat (arm64,x64)\" "
                               "\"3\"  \"Thunderbird (雷鸟,Mozilla开发的email客户端)\" "
                               "\"4\"  \"Kmail (KDE邮件客户端)\" "
                               "\"5\"  \"Evolution (GNOME邮件客户端)\" "
                               "\"6\"  \"Empathy (GNOME多协议语音视频聊天)\" "
                               "\"7\"  \"Pidgin (IM即时通讯软件)\" "
                               "\"8\"  \"Xchat (IRC客户端,类似Amiga的AmIRC)\" "
                               "\"9\"  \"Skype (x64,微软出品的IM软件)\" "
                               "\"10\" \"米聊 (x64,小米科技出品的即时通讯工具)\" "
                               "\"0\"  \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;

            if (ch == "1") install_linux_qq();
            else if (ch == "2") install_wechat();
            else if (ch == "3") PackageManager::install("thunderbird", family);
            else if (ch == "4") PackageManager::install("kmail", family);
            else if (ch == "5") PackageManager::install("evolution", family);
            else if (ch == "6") PackageManager::install("empathy", family);
            else if (ch == "7") PackageManager::install("pidgin", family);
            else if (ch == "8") PackageManager::install("xchat", family);
            else if (ch == "9") install_skype();
            else if (ch == "10") install_mitalk();
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 6. 🎮 Games — 对应旧 Bash tmoe_games_menu (8项)
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_games_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.games_menu")
                           + "\" --menu \"Which game do you want to install?\" 0 0 0 "
                           "\"1\" \"🎮 KDE-games (KDE项目的小游戏合集)\" "
                           "\"2\" \"👣 GNOME-games\" "
                           "\"3\" \"🤓 Steam-x86_64 (蒸汽游戏平台)\" "
                           "\"4\" \"cataclysm-大灾变 (末日幻想探索生存游戏)\" "
                           "\"5\" \"wesnoth韦诺之战 (奇幻回合制策略战棋)\" "
                           "\"6\" \"retroarch (全能复古游戏模拟器)\" "
                           "\"7\" \"dolphin-emu (任天堂wii模拟器)\" "
                           "\"8\" \"SuperTuxKart (3D卡丁车)\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install({"kdegames", "kde-full"}, family);
        else if (ch == "2") PackageManager::install("gnome-games", family);
        else if (ch == "3") PackageManager::install("steam-launcher", family);
        else if (ch == "4") PackageManager::install("cataclysm-dda", family);
        else if (ch == "5") PackageManager::install("wesnoth", family);
        else if (ch == "6") PackageManager::install("retroarch", family);
        else if (ch == "7") PackageManager::install("dolphin-emu", family);
        else if (ch == "8") PackageManager::install("supertuxkart", family);
        Logger::press_enter();
    }

    // ═══════════════════════════════════════════════════════════════
    // 7. 📚 Documents → office_cb_ 重定向
    // 8. 🏤 Debian Opt Repo — 对应旧 Bash explore_debian_opt_repo
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_debian_opt_menu() {
        std::string menu = cfg_.tui_bin + " --title \"🏤 DEBIAN OPT REPO\""
                           " --menu \"Only supports DEBIAN-based distros\" 0 0 0 "
                           "\"1\"  \"🎶 music: vocal, flacon\" "
                           "\"2\"  \"📝 notes笔记: markdown编辑器\" "
                           "\"3\"  \"📺 videos视频: 多媒体音视频转换\" "
                           "\"4\"  \"🖼️ pictures图像: bing壁纸\" "
                           "\"5\"  \"📖 reader: 悦享生活,品味阅读\" "
                           "\"6\"  \"🎮 games游戏: Minecraft启动器\" "
                           "\"7\"  \"👾 development: GUI设计\" "
                           "\"8\"  \"💠 tools工具: winetricks-zh\" "
                           "\"9\"  \"🔯 internet: motrix, freechat\" "
                           "\"0\"  \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        Logger::info("Debian Opt Repo: 功能待细化 (对应旧 Bash deprecated/debian-opt)");
        // 旧 Bash 中每个子项有独立的 install_* 函数，包名待补全
        Logger::press_enter();
    }

    // ═══════════════════════════════════════════════════════════════
    // 9. 🎁 Download → download_cb_ 重定向
    // 10. 🔯 Packages & System — 对应旧 Bash tmoe_software_package_menu
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_pkg_gui_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.pkg_gui_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("swcenter.synaptic") + "\" "
                           "\"2\" \"" + _("swcenter.gdebi") + "\" "
                           "\"3\" \"" + _("swcenter.pamac") + "\" "
                           "\"4\" \"" + _("swcenter.bleachbit") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install("synaptic", family);
        else if (ch == "2") PackageManager::install("gdebi", family);
        else if (ch == "3") PackageManager::install("pamac", family);
        else if (ch == "4") PackageManager::install("bleachbit", family);
        Logger::press_enter();
    }

    // ═══════════════════════════════════════════════════════════════
    // 11. 🥙 Zsh → zsh_cb_ 重定向
    // 12. 🥗 File Shared — 对应旧 Bash personal_netdisk
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_fileshare_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.fileshare_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("swcenter.filebrowser") + "\" "
                           "\"2\" \"" + _("swcenter.nginx_webdav") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") {
            Logger::info(_("swcenter.downloading") + ": FileBrowser");
            Executor::passthrough(
                "curl -fsSL https://raw.githubusercontent.com/filebrowser/get/master/get.sh | bash || true"
            );
        } else if (ch == "2") {
            PackageManager::install("nginx", family);
        }
        Logger::press_enter();
    }

    // ═══════════════════════════════════════════════════════════════
    // 13. 💔 Remove — 对应旧 Bash tmoe_other_options_menu
    // ═══════════════════════════════════════════════════════════════
    void SoftwareCenter::run_cleanup_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"Removable items\""
                               " --menu \"Which item do you want to remove?\" 0 0 0 "
                               "\"1\" \"" + _("swcenter.cleanup_rm_gui") + "\" "
                               "\"2\" \"" + _("swcenter.cleanup_rm_browser") + "\" "
                               "\"3\" \"" + _("swcenter.cleanup_rm_tmoe") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") {
                remove_gui();
            } else if (ch == "2") {
                remove_browser();
            } else if (ch == "3") {
                remove_tmoe_tools();
            }
            Logger::press_enter();
        }
    }

    void SoftwareCenter::remove_gui() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        Logger::step("Removing GUI desktop environments...");
        auto confirm = Executor::passthrough(cfg_.tui_bin +
                                             " --yesno \"按回车键确认卸载所有桌面环境\" 0 0");
        if (confirm.exit_code != 0) return;

        switch (family) {
            case DistroFamily::Debian:
                PackageManager::remove({
                    "xfce4", "xfce4-terminal", "tightvncserver", "xfce4-goodies",
                    "dbus-x11", "lxde-core", "lxterminal",
                    "mate-desktop-environment-core", "mate-terminal",
                    "kde-plasma-desktop", "dde", "dde-desktop",
                    "cinnamon-desktop-environment", "gnome-session", "gnome-shell",
                    "lxqt", "lxqt-qtplugin"
                }, family);
                Executor::passthrough("apt autoremove --purge -y || apt autoremove -y");
                break;
            case DistroFamily::Arch:
                PackageManager::remove({
                    "xfce4", "xfce4-goodies", "mate", "mate-extra",
                    "lxde", "lxqt", "plasma-desktop", "gnome", "gnome-extra",
                    "cinnamon", "deepin", "deepin-extra"
                }, family);
                break;
            case DistroFamily::RedHat:
                for (const auto &grp: {
                         "xfce", "mate-desktop", "lxde-desktop", "lxqt",
                         "KDE", "GNOME", "Cinnamon Desktop"
                     })
                    Executor::passthrough("dnf groupremove -y \"" + std::string(grp) + "\" 2>/dev/null || true");
                PackageManager::remove("deepin-desktop", family);
                break;
            default:
                PackageManager::remove({
                    "xfce4", "lxde", "lxqt", "mate-desktop",
                    "kde-plasma-desktop", "gnome-session", "cinnamon", "dde"
                }, family);
                break;
        }
    }

    void SoftwareCenter::remove_browser() {
        // 对应旧 Bash: 火狐娘 vs chromium娘 二选一
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string confirm = cfg_.tui_bin +
                              " --title \"请从两个小可爱中选择一个\" --yes-button \"Firefox\" --no-button \"chromium\""
                              " --yesno '火狐娘:\"虽然知道总有离别时，但我没想到这一天竟然会这么早。\""
                              "chromium娘:\"哼，负心人，走了之后就别回来了！\""
                              " 请做出您的选择！' 10 60";
        auto choice = Executor::passthrough(confirm);
        // exit_code: 0=Firefox(yes), 1=chromium(no), 255=ESC

        if (choice.exit_code == 0) {
            auto f_confirm = Executor::passthrough(cfg_.tui_bin +
                                                   " --yesno \"按回车键确认卸载 Firefox\" 0 0");
            if (f_confirm.exit_code != 0) return;
            PackageManager::remove({
                "firefox-esr", "firefox-esr-l10n-zh-cn",
                "firefox", "firefox-l10n-zh-cn", "firefox-locale-zh-hans"
            }, family);
        } else if (choice.exit_code == 1) {
            auto c_confirm = Executor::passthrough(cfg_.tui_bin +
                                                   " --yesno \"按回车键确认卸载 Chromium\" 0 0");
            if (c_confirm.exit_code != 0) return;
            Executor::passthrough(
                "apt-mark unhold chromium-browser chromium-browser-l10n chromium-codecs-ffmpeg-extra 2>/dev/null || true");
            PackageManager::remove({
                "chromium", "chromium-l10n",
                "chromium-browser", "chromium-browser-l10n"
            }, family);
        }
        Executor::passthrough("apt autoremove --purge -y 2>/dev/null || true");
    }

    void SoftwareCenter::remove_tmoe_tools() {
        Logger::step("Removing tmoe tools...");
        Logger::warn("WARNING！删除 ~/.config/tmoe-linux 将导致容器无法正常移除，建议先移除容器再删配置。");

        auto tm_confirm = Executor::passthrough(cfg_.tui_bin +
                                                " --yesno \"确认卸载 tmoe-linux 管理器？\n此操作不可逆！\" 0 0");
        if (tm_confirm.exit_code != 0) return;

        CommandBuilder("rm")
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
        PackageManager::remove({"git", "aria2", "pv", "wget", "curl", "less", "xz-utils", "newt", "whiptail"}, family);

        Logger::warn("tmoe 工具已移除。容器数据保留在 ~/.local/share/tmoe");
    }
} // namespace tmoe::domain
