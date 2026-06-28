#include "domain/system/termux.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace tmoe::domain {
    // ═══════════════════════════════════════════════════════════════
    // 环境初始化 (已完成)
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::check_and_init_environment() {
        if (!cfg_.is_termux) return true;

        Logger::step(_("termux.checking_env"));
        std::string missing_pkgs;

        // 核心依赖
        if (!Executor::has("curl") && !Executor::has("wget")) missing_pkgs += " curl wget";
        if (!Executor::has("git")) missing_pkgs += " git";
        if (!Executor::has("proot")) missing_pkgs += " proot";
        if (!Executor::has("tar") || !Executor::has("xz")) missing_pkgs += " tar xz-utils";

        // 扩展依赖检测
        if (!Executor::has("lsof")) missing_pkgs += " " + map_binary_to_termux_pkg("lsof");
        if (!Executor::has("pkill")) missing_pkgs += " " + map_binary_to_termux_pkg("pkill");
        if (!Executor::has("chroot")) missing_pkgs += " " + map_binary_to_termux_pkg("chroot");
        if (!Executor::has("unshare")) missing_pkgs += " " + map_binary_to_termux_pkg("unshare");

        // 可选依赖提示
        if (!Executor::has("bat") && !Executor::has("batcat")) Logger::info(_("termux.hint_bat"));
        if (!Executor::has("micro")) Logger::info(_("termux.hint_micro"));

        if (!missing_pkgs.empty()) {
            Logger::warn(_("termux.missing_deps") + ": " + missing_pkgs);
            Executor::shell(cfg_.update_command);
            if (!Executor::shell(cfg_.install_command + missing_pkgs).ok()) {
                Logger::error(_("termux.deps_install_failed"));
                return false;
            }
            Logger::ok(_("termux.deps_install_ok"));
        }

        // openssl 旧版检测
        check_openssl_legacy();

        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        std::string termux_dir = home + "/.termux";
        fs::create_directories(termux_dir);

        if (!fs::exists(termux_dir + "/colors.properties")) termux_color_scheme_menu();
        if (!fs::exists(termux_dir + "/font.ttf")) termux_font_menu();
        configure_extra_keys();
        Executor::shell("termux-reload-settings");
        return true;
    }

    bool TermuxManager::setup_storage() {
        if (!cfg_.is_termux) return true;

        Logger::step(_("termux.checking_storage"));

        // 检查 storage/shared 软链接是否已存在
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        if (fs::exists(home + "/storage/shared")) {
            Logger::info(_("termux.storage_already_set"));
            return true;
        }

        Logger::warn(_("termux.storage_not_set"));
        if (!Executor::has("termux-setup-storage")) {
            Logger::step(_("termux.installing_tools"));
            PackageManager::update(DistroFamily::Debian);
            PackageManager::install("termux-tools", DistroFamily::Debian);
        }

        auto result = Executor::shell("termux-setup-storage");
        if (result.ok()) {
            Logger::ok(_("termux.storage_setup_ok"));
            return true;
        }

        Logger::error(_("termux.storage_setup_failed"));
        return false;
    }

    // ═══════════════════════════════════════════════════════════════
    // X11 / GUI 支持
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::install_x11_support() {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.skip_x11"));
            return true;
        }

        Logger::step(_("termux.installing_x11"));

        std::string desktop = select_desktop_environment();
        if (desktop.empty()) {
            Logger::info(_("termux.gui_cancelled"));
            return false;
        }

        if (desktop == "xfce") return install_termux_xfce();
        if (desktop == "lxqt") return install_termux_lxqt();

        return false;
    }

    std::string TermuxManager::select_desktop_environment() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.gui_title") + "\" "
                          "--menu \"" + _("termux.de_select_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.de_xfce") + "\" "
                          "\"2\" \"" + _("termux.de_lxqt") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto result = Executor::tui_select(cmd);
        if (result == "1") return "xfce";
        if (result == "2") return "lxqt";
        return "";
    }

    bool TermuxManager::install_termux_xfce() {
        Logger::step(_("termux.installing_xfce4"));

        // 1. 安装 x11-repo
        PackageManager::update(DistroFamily::Debian);
        PackageManager::install("x11-repo", DistroFamily::Debian);
        PackageManager::update(DistroFamily::Debian);
        Executor::shell("apt dist-upgrade -y");

        // 2. 安装 XFCE4 依赖
        PackageManager::install({"xfce", "aterm", "xfce4-terminal", "tigervnc"}, DistroFamily::Debian);

        // 3. 创建 startvnc 启动脚本
        std::string resolution = select_vnc_resolution();
        create_startvnc_script(resolution, "xfce");

        Logger::ok(_("termux.xfce4_installed"));
        Logger::info(_("termux.vnc_connect_info"));
        return true;
    }

    bool TermuxManager::install_termux_lxqt() {
        Logger::step(_("termux.installing_lxqt"));

        PackageManager::update(DistroFamily::Debian);
        PackageManager::install("x11-repo", DistroFamily::Debian);
        PackageManager::update(DistroFamily::Debian);
        Executor::shell("apt dist-upgrade -y");

        PackageManager::install({"lxqt", "qterminal", "tigervnc"}, DistroFamily::Debian);

        std::string resolution = select_vnc_resolution();
        create_startvnc_script(resolution, "lxqt");

        Logger::ok(_("termux.lxqt_installed"));
        Logger::info(_("termux.vnc_connect_info"));
        return true;
    }

    std::string TermuxManager::select_vnc_resolution() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.vnc_res_title") + "\" "
                          "--menu \"" + _("termux.vnc_res_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.vnc_res_hd") + "\" "
                          "\"2\" \"" + _("termux.vnc_res_fhd") + "\" "
                          "\"3\" \"" + _("termux.vnc_res_portrait") + "\" "
                          "\"4\" \"" + _("termux.vnc_res_2k") + "\" "
                          "\"5\" \"" + _("termux.vnc_res_custom") + "\"";

        auto result = Executor::tui_select(cmd);

        if (result == "1") return "1280x720";
        if (result == "2") return "1920x1080";
        if (result == "3") return "720x1440";
        if (result == "4") return "2560x1440";
        if (result == "5") {
            std::string input_cmd = cfg_.tui_bin + " --title \"" + _("termux.vnc_custom_title") + "\" "
                                    "--inputbox \"" + _("termux.vnc_custom_input") + "\" 0 50";
            auto custom = Executor::tui_select(input_cmd);
            if (!custom.empty()) return custom;
        }
        return "1280x720"; // default
    }

    void TermuxManager::create_startvnc_script(const std::string &resolution, const std::string &desktop_env) {
        std::string bin_dir = "/data/data/com.termux/files/usr/bin";
        std::string script_path = bin_dir + "/startvnc";

        std::ofstream ofs(script_path);
        if (!ofs.is_open()) {
            Logger::error(_f("termux.startvnc_create_failed", script_path));
            return;
        }

        std::string terminal = configure_terminal_emulator_for_de(desktop_env);
        std::string xsession = desktop_env;
        std::string xsession_02 = get_fallback_xsession(desktop_env);

        // 主会话命令
        std::string session_cmd;
        if (desktop_env == "xfce") {
            session_cmd = "dbus-launch --exit-with-session startxfce4 &";
        } else if (desktop_env == "lxqt") {
            session_cmd = "dbus-launch --exit-with-session startlxqt &";
        } else if (desktop_env == "kde" || desktop_env == "plasma") {
            session_cmd = "dbus-launch --exit-with-session startplasma-x11 &";
        } else if (desktop_env == "gnome") {
            session_cmd = "dbus-launch --exit-with-session gnome-session &";
        } else if (desktop_env == "mate") {
            session_cmd = "dbus-launch --exit-with-session mate-session &";
        } else if (desktop_env == "cinnamon") {
            session_cmd = "dbus-launch --exit-with-session cinnamon-session &";
        } else {
            session_cmd = "dbus-launch --exit-with-session " + desktop_env + " &";
        }

        ofs << "#!/data/data/com.termux/files/usr/bin/bash\n";
        ofs << "# Auto-generated by tmoe-cpp\n";
        ofs << "export DISPLAY=:2\n";
        ofs << "VNC_RESOLUTION=" << resolution << "\n";
        ofs << "export PULSE_SERVER=127.0.0.1\n\n";

        ofs << "# Kill old VNC server\n";
        ofs << "pkill Xvnc 2>/dev/null\n";
        ofs << "sleep 1\n\n";

        ofs << "# Start PulseAudio if available\n";
        ofs << "pulseaudio --start 2>/dev/null\n\n";

        ofs << "# Start VNC server\n";
        ofs << "Xvnc -geometry ${VNC_RESOLUTION} -depth 24 \\\n";
        ofs << "     -desktop termux-" << desktop_env << " \\\n";
        ofs << "     --SecurityTypes=None ${DISPLAY} &\n\n";
        ofs << "sleep 2\n\n";

        ofs << "# Auto-start RealVNC viewer on Android\n";
        ofs <<
                "am start -n com.realvnc.viewer.android/com.realvnc.viewer.android.app.ConnectionChooserActivity 2>/dev/null\n\n";

        ofs << "# Auto-start file manager\n";
        ofs << "for fm in thunar pcmanfm-qt nautilus dolphin; do\n";
        ofs << "    if command -v ${fm} >/dev/null 2>&1; then\n";
        ofs << "        ${fm} &\n";
        ofs << "        break\n";
        ofs << "    fi\n";
        ofs << "done\n\n";

        ofs << "# Auto-start terminal\n";
        ofs << "if command -v " << terminal << " >/dev/null 2>&1; then\n";
        ofs << "    " << terminal << " &\n";
        ofs << "elif command -v xfce4-terminal >/dev/null 2>&1; then\n";
        ofs << "    xfce4-terminal &\n";
        ofs << "elif command -v qterminal >/dev/null 2>&1; then\n";
        ofs << "    qterminal &\n";
        ofs << "fi\n\n";

        ofs << "# Start desktop session (with fallback)\n";
        ofs << session_cmd << "\n\n";

        if (!xsession_02.empty()) {
            ofs << "# Fallback session if primary fails\n";
            ofs << "sleep 2\n";
            ofs << "if ! pgrep -f \"" << xsession << "\" >/dev/null 2>&1; then\n";
            ofs << "    echo \"Primary session failed, trying fallback...\"\n";
            ofs << "    " << xsession_02 << " &\n";
            ofs << "fi\n\n";
        }

        ofs << "# Show connection info\n";
        ofs << "echo \"=================================\"\n";
        ofs << "echo \"  VNC server running at 127.0.0.1:5902\"\n";
        ofs << "echo \"  使用 VNC 客户端连接即可。\"\n";
        ofs << "echo \"=================================\"\n";

        // 局域网 IP 检测
        ofs << "echo \"\"\n";
        ofs << "echo \"📡 局域网 VNC 地址:\"\n";
        ofs << "ip -4 -br -c a 2>/dev/null | grep -v '127.0.0.1' | head -3\n";
        ofs << "echo \"=================================\"\n\n";

        ofs << "wait\n";

        ofs.close();
        CommandBuilder("chmod").add_flag("+x").add_arg(script_path).execute();

        // 运行 termux-fix-shebang
        run_termux_fix_shebang(script_path);

        Logger::ok(_f("termux.startvnc_created", script_path));
    }

    bool TermuxManager::configure_termux_vnc() {
        if (!cfg_.is_termux) return true;

        std::string startvnc = "/data/data/com.termux/files/usr/bin/startvnc";
        if (!fs::exists(startvnc)) {
            Logger::warn(_("termux.startvnc_not_found"));
            return false;
        }

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.vnc_conf_title") + "\" "
                          "--menu \"" + _("termux.vnc_conf_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.vnc_conf_resolution") + "\" "
                          "\"2\" \"" + _("termux.vnc_conf_edit") + "\" "
                          "\"3\" \"" + _("termux.vnc_conf_lan") + "\" "
                          "\"4\" \"" + _("termux.vnc_conf_fix") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") {
            std::string new_resolution = select_vnc_resolution();
            if (new_resolution.empty()) return false;
            std::string sed_cmd = "sed -i -E 's@^(VNC_RESOLUTION=).*@\\1" + new_resolution + "@' " + startvnc;
            Executor::shell(sed_cmd);
            Logger::ok(_f("termux.vnc_resolution_updated", new_resolution));
        } else if (choice == "2") {
            edit_vnc_config_manually();
        } else if (choice == "3") {
            detect_and_show_lan_ip();
        } else if (choice == "4") {
            run_termux_fix_shebang(startvnc);
            Logger::ok(_("termux.startvnc_fixed"));
        }

        return true;
    }

    bool TermuxManager::remove_termux_gui() {
        if (!cfg_.is_termux) return true;

        std::string confirm = cfg_.tui_bin + " --title \"" + _("termux.remove_gui_title") + "\" "
                              "--yesno \"" + _("termux.remove_gui_confirm") + "\" 0 50";

        if (!Executor::shell(confirm).ok()) {
            Logger::info(_("termux.gui_remove_cancelled"));
            return false;
        }

        Logger::step(_("termux.removing_gui"));
        PackageManager::remove("^xfce", DistroFamily::Debian);
        PackageManager::remove("tigervnc", DistroFamily::Debian);
        PackageManager::remove("aterm", DistroFamily::Debian);
        PackageManager::remove("xfce4-terminal", DistroFamily::Debian);
        PackageManager::remove("lxqt", DistroFamily::Debian);
        PackageManager::remove("qterminal", DistroFamily::Debian);
        PackageManager::remove("x11-repo", DistroFamily::Debian);
        Executor::shell("apt autoremove --purge -y");

        // 清理 startvnc 脚本
        std::string startvnc = "/data/data/com.termux/files/usr/bin/startvnc";
        if (fs::exists(startvnc)) fs::remove(startvnc);

        Logger::ok(_("termux.gui_removed"));
        return true;
    }

    int TermuxManager::run_termux_gui_menu() {
        while (true) {
            std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.gui_mgmt_title") + "\" "
                              "--menu \"" + _("termux.gui_mgmt_prompt") + "\" 0 50 0 "
                              "\"1\" \"" + _("termux.gui_xfce4") + "\" "
                              "\"2\" \"" + _("termux.gui_lxqt") + "\" "
                              "\"3\" \"" + _("termux.gui_resolution") + "\" "
                              "\"4\" \"" + _("termux.gui_remove") + "\" "
                              "\"0\" \"" + _("menu.tui.back") + "\"";

            auto choice = Executor::tui_select(cmd);
            if (choice == "1") install_termux_xfce();
            else if (choice == "2") install_termux_lxqt();
            else if (choice == "3") configure_termux_vnc();
            else if (choice == "4") remove_termux_gui();
            else if (choice == "0" || choice.empty()) return 0;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 备份与还原
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::backup_termux_prepare(std::string &out_dir, std::string &out_filename,
                                              std::string &out_timestamp) {
        out_dir = "/sdcard/Download/backup/termux";
        fs::create_directories(out_dir);

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M", &tm_buf);
        out_timestamp = buf;
    }

    std::string TermuxManager::get_backup_filename() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.backup_name_title") + "\" "
                          "--inputbox \"" + _("termux.backup_name_input") + "\" 0 50 \"termux-backup\"";

        auto name = Executor::tui_select(cmd);
        if (name.empty()) name = "termux-backup";
        return name;
    }

    std::string TermuxManager::select_backup_directories() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.backup_dir_title") + "\" "
                          "--checklist \"" + _("termux.backup_dir_prompt") + "\" 0 50 0 "
                          "\"home\" \"" + _("termux.backup_home") + "\" ON "
                          "\"usr\" \"" + _("termux.backup_usr") + "\" OFF "
                          "3>&1 1>&2 2>&3";

        return Executor::tui_select(cmd);
    }

    bool TermuxManager::backup_termux() {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.skip_backup"));
            return true;
        }

        Logger::step(_("termux.backup_title"));

        // 1. 选择备份目录
        std::string selected = select_backup_directories();
        if (selected.empty()) {
            Logger::info(_("termux.backup_cancelled"));
            return false;
        }

        // 2. 准备
        std::string backup_dir, filename, timestamp;
        backup_termux_prepare(backup_dir, filename, timestamp);

        filename = get_backup_filename();
        if (filename.empty()) return false;

        // 3. 选择压缩类型
        std::string compress_cmd = cfg_.tui_bin + " --title \"" + _("termux.compress_title") + "\" "
                                   "--yesno \"" + _("termux.compress_yesno") + "\" 0 50 "
                                   "--yes-button \"" + _("termux.compress_zstd") + "\" --no-button \"" + _(
                                       "termux.compress_xz") + "\"";

        bool use_zstd = Executor::shell(compress_cmd).ok();

        // 4. 构建排除和包含路径
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string tmoe_share = cfg_.work_dir.string();

        std::vector<std::string> backup_dirs;
        if (selected.find("home") != std::string::npos) backup_dirs.push_back(home);
        if (selected.find("usr") != std::string::npos) backup_dirs.push_back(prefix);

        std::string tar_targets;
        for (auto &d: backup_dirs) tar_targets += " " + d;

        std::string tar_file;
        if (use_zstd) {
            tar_file = backup_dir + "/" + filename + "-" + timestamp + ".tar.zst";
            Logger::step(_("termux.backup_compressing_zstd"));
            Executor::shell("tar --use-compress-program zstd -Ppvcf " + tar_file +
                            " --exclude=" + tmoe_share + "/containers" + tar_targets);
        } else {
            tar_file = backup_dir + "/" + filename + "-" + timestamp + ".tar.xz";
            Logger::step(_("termux.backup_compressing_xz"));
            Executor::shell("tar -PJpvcf " + tar_file +
                            " --exclude=" + tmoe_share + "/containers" + tar_targets);
        }

        if (fs::exists(tar_file)) {
            auto fsize = fs::file_size(tar_file);
            Logger::ok(_f("termux.backup_complete", tar_file,
                          std::to_string(fsize / 1024 / 1024) + " MB)"));
            return true;
        }

        Logger::error(_("termux.backup_failed"));
        return false;
    }

    // ── 还原 ──

    std::string TermuxManager::detect_latest_backup() {
        std::string backup_dir = "/sdcard/Download/backup/termux";
        if (!fs::exists(backup_dir)) return "";

        auto result = Executor::shell("ls -1t " + backup_dir + "/*termux*bak.tar* 2>/dev/null | head -1");
        std::string path = result.stdout_data;
        // trim trailing newline
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        return path;
    }

    std::string TermuxManager::select_backup_file_manually() {
        std::string backup_dir = "/sdcard/Download/backup/termux";
        if (!fs::exists(backup_dir)) {
            Logger::error(_f("termux.backup_dir_not_found", backup_dir));
            return "";
        }

        // 列出所有备份文件
        Logger::step(_("termux.scanning_backups"));
        std::string ls_result = Executor::shell("ls -1th " + backup_dir + "/*termux*bak.tar* 2>/dev/null").stdout_data;

        if (ls_result.empty()) {
            Logger::error(_("termux.no_backup_found"));
            return "";
        }

        // 构建 whiptail 菜单
        std::stringstream menu_builder;
        menu_builder << cfg_.tui_bin << " --title \"" + _("termux.select_backup_title") + "\" "
                << "--menu \"" + _("termux.select_backup_prompt") + "\" 0 50 20 ";

        std::stringstream ss(ls_result);
        std::string line;
        int idx = 1;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            // 提取文件名
            size_t pos = line.find_last_of('/');
            std::string fname = (pos != std::string::npos) ? line.substr(pos + 1) : line;
            menu_builder << "\"" << idx << "\" \"" << fname << "\" ";
            idx++;
        }
        menu_builder << "\"0\" \"" + _("menu.tui.back") + "\"";
        menu_builder << " 3>&1 1>&2 2>&3";

        auto choice = Executor::tui_select(menu_builder.str());
        if (choice == "0" || choice.empty()) return "";

        int idx_c = std::stoi(choice);
        ss.clear();
        ss.seekg(0);
        while (std::getline(ss, line)) {
            if (--idx_c == 0) {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
                return line;
            }
        }
        return "";
    }

    bool TermuxManager::restore_termux(std::string_view archive_path) {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.skip_restore"));
            return true;
        }

        Logger::step(_("termux.restore_title"));

        std::string restore_file;
        if (!archive_path.empty()) {
            restore_file = archive_path;
        } else {
            // 检查是否有最新备份
            std::string latest = detect_latest_backup();
            if (!latest.empty()) {
                std::string confirm = cfg_.tui_bin + " --title \"" + _("termux.restore_title") + "\" "
                                      "--yesno \"" + _f("termux.restore_confirm_latest", latest) + "\" 0 50";
                if (Executor::shell(confirm).ok()) {
                    restore_file = latest;
                } else {
                    restore_file = select_backup_file_manually();
                }
            } else {
                restore_file = select_backup_file_manually();
            }
        }

        if (restore_file.empty()) {
            Logger::info(_("termux.restore_cancelled"));
            return false;
        }

        // 警告
        std::string warn = cfg_.tui_bin + " --title \"" + _("termux.warning_title") + "\" "
                           "--yesno \"" + _("termux.restore_warning") + "\" 0 50";
        if (!Executor::shell(warn).ok()) {
            Logger::info(_("termux.restore_cancelled"));
            return false;
        }

        return uncompress_restore_archive(restore_file);
    }

    bool TermuxManager::uncompress_restore_archive(const std::string &archive_path) {
        if (archive_path.find(".zst") != std::string::npos) {
            return uncompress_zst(archive_path);
        } else {
            return uncompress_xz(archive_path);
        }
    }

    bool TermuxManager::uncompress_zst(const std::string &file) {
        Logger::step(_f("termux.restore_extracting_zst", file));

        if (Executor::has("pv")) {
            Executor::shell("pv " + file + " | tar --use-compress-program zstd -Ppx");
        } else {
            Executor::shell("tar --use-compress-program zstd -Ppxvf " + file);
        }

        Logger::ok(_("termux.restore_complete"));
        return true;
    }

    bool TermuxManager::uncompress_xz(const std::string &file) {
        Logger::step(_f("termux.restore_extracting_xz", file));

        if (Executor::has("pv")) {
            Executor::shell("pv " + file + " | tar -PpJx");
        } else {
            Executor::shell("tar -PpJxvf " + file);
        }

        Logger::ok(_("termux.restore_complete"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 终端美化
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::beautify_terminal() {
        Logger::step(_("termux.beautify_title"));

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.beautify_title") + "\" "
                          "--menu \"" + _("termux.beautify_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.beautify_tmoe_zsh") + "\" "
                          "\"2\" \"" + _("termux.beautify_ohmyzsh") + "\" "
                          "\"3\" \"" + _("termux.beautify_p10k") + "\" "
                          "\"4\" \"" + _("termux.beautify_colorls") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") return configure_tmoe_zsh();
        if (choice == "2") {
            Logger::step(_("termux.installing_ohmyzsh"));
            PackageManager::install({"zsh", "git", "curl"}, DistroFamily::Debian);
            Executor::shell(
                "sh -c \"$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\" \"\" --unattended");
            Logger::ok(_("termux.ohmyzsh_installed"));
            return true;
        }
        if (choice == "3") {
            Logger::step(_("termux.installing_p10k"));
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
            std::string p10k_dir = home + "/.oh-my-zsh/custom/themes/powerlevel10k";
            if (!fs::exists(p10k_dir)) {
                Executor::shell("git clone --depth=1 https://github.com/romkatv/powerlevel10k.git " + p10k_dir);
            }
            // 更新 .zshrc 中的 ZSH_THEME
            std::string zshrc = home + "/.zshrc";
            if (fs::exists(zshrc)) {
                Executor::shell("sudo sed -i 's/^ZSH_THEME=.*/ZSH_THEME=\"powerlevel10k\\/powerlevel10k\"/' " + zshrc);
            }
            Logger::ok(_("termux.p10k_installed"));
            return true;
        }
        if (choice == "4") {
            Logger::step(_("termux.installing_colorls"));
            PackageManager::install("ruby", DistroFamily::Debian);
            Executor::shell("gem install colorls");
            Logger::ok(_("termux.colorls_installed"));
            return true;
        }
        return false;
    }

    bool TermuxManager::configure_tmoe_zsh() {
        Logger::step(_("termux.configuring_tmoe_zsh"));

        // 检测是否已配置
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        std::string git_config = home + "/.config/tmoe-zsh/git/.git/config";

        if (fs::exists(git_config)) {
            Logger::info(_("termux.tmoe_zsh_configured"));
            return true;
        }

        // 安装依赖
        std::string confirm = cfg_.tui_bin + " --title \"" + _("termux.zsh_title") + "\" "
                              "--yesno \"" + _("termux.zsh_confirm") + "\" 0 50";
        if (!Executor::shell(confirm).ok()) {
            Logger::info(_("termux.tmoe_zsh_cancelled"));
            return false;
        }

        // 下载并运行安装脚本
        std::string install_bin = "/data/data/com.termux/files/usr/local/bin/zsh-i";
        std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/zsh.sh";

        Logger::step(_("termux.downloading_tmoe_zsh"));
        CommandBuilder("mkdir").add_flag("-p").add_arg("/data/data/com.termux/files/usr/local/bin").execute();
        CommandBuilder("curl").add_flag("-Lo").add_arg(install_bin).add_arg(url).execute();
        CommandBuilder("chmod").add_arg("777").add_arg(install_bin).execute();

        Logger::step(_("termux.running_tmoe_zsh_auto"));
        Executor::shell("bash " + install_bin + " --tmoe_container_auto_configure");

        Logger::ok(_("termux.tmoe_zsh_complete"));
        return true;
    }

    bool TermuxManager::start_tmoe_zsh() {
        // 对应 original bash: start_tmoe_zsh_manager() in old/tools/app/center
        // 启动外部 tmoe-zsh TUI 管理脚本 (https://github.com/2cd/zsh)
        const std::string zsh_tool_url = "https://raw.githubusercontent.com/2cd/zsh/master/zsh.sh";
        const char* home_env = std::getenv("HOME");
        std::string home = home_env ? home_env : "/root";
        std::string local_zsh_script = home + "/.config/tmoe-zsh/git/zsh.sh";

        // 优先级1: 已安装的 zsh-i 命令
        if (Executor::has("zsh-i")) {
            Logger::step(_("termux.tmoe_zsh_via_zsh_i"));
            Executor::passthrough("zsh-i");
            return true;
        }

        // 优先级2: 本地已缓存的脚本
        if (fs::exists(local_zsh_script)) {
            Logger::step(_("termux.tmoe_zsh_via_local"));
            Executor::passthrough("bash " + local_zsh_script);
            return true;
        }

        // 优先级3: 从 GitHub 下载并运行
        const char* sudo_user = std::getenv("SUDO_USER");
        bool is_root_home = (home == "/root");
        bool needs_su = is_root_home && sudo_user && !cfg_.is_termux;

        if (needs_su) {
            Logger::step(_f("termux.tmoe_zsh_via_su", std::string(sudo_user)));
            CommandBuilder("curl").add_flag("-Lo").add_arg("/tmp/.zsh-i.sh").add_arg(zsh_tool_url).execute();
            CommandBuilder("chmod").add_arg("a+rx").add_arg("/tmp/.zsh-i.sh").execute();
            Executor::passthrough("su - " + std::string(sudo_user) + " -c 'bash /tmp/.zsh-i.sh'");
        } else {
            Logger::step(_("termux.tmoe_zsh_starting"));
            Executor::passthrough(
                "bash -c \"$(curl -LfsS " + zsh_tool_url + ")\""
            );
        }

        Logger::ok(_("termux.tmoe_zsh_exited"));
        return true;
    }

    bool TermuxManager::change_shell_to_zsh() {
        if (!Executor::has("zsh")) {
            Logger::warn(_("termux.zsh_not_installed"));
            PackageManager::install("zsh", DistroFamily::Debian);
        }

        // 检查当前 shell
        auto current = Executor::shell("echo $SHELL");
        if (current.stdout_data.find("zsh") != std::string::npos) {
            Logger::info(_("termux.already_zsh"));
            return true;
        }

        if (cfg_.is_termux) {
            // Termux 使用 chsh
            Executor::shell("chsh -s zsh");
        } else {
            // 容器内修改 /etc/passwd
            Executor::shell("sudo sed -E -i 's@(root:x:0:0:root:/root:/bin/)(ash|bash)@\\1zsh@' /etc/passwd");
        }

        Logger::ok(_("termux.shell_switched_zsh"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 软件包镜像源
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::switch_pkg_mirror(std::string_view mirror) {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.skip_mirror"));
            return true;
        }

        // 如果已指定 mirror，直接切换
        if (!mirror.empty()) {
            std::string url = std::string(mirror);
            if (url.find("://") == std::string::npos) {
                url = "https://" + url + "/termux/apt";
            }
            modify_termux_sources_list(url);
            return true;
        }

        // 否则显示 TUI 菜单
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.mirror_title") + "\" "
                          "--menu \"" + _("termux.mirror_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.mirror_bfsu") + "\" "
                          "\"2\" \"" + _("termux.mirror_tencent") + "\" "
                          "\"3\" \"" + _("termux.mirror_tsinghua") + "\" "
                          "\"4\" \"" + _("termux.mirror_ustc") + "\" "
                          "\"5\" \"" + _("termux.mirror_repos") + "\" "
                          "\"6\" \"" + _("termux.mirror_speed_test") + "\" "
                          "\"7\" \"" + _("termux.mirror_edit") + "\" "
                          "\"8\" \"" + _("termux.mirror_clean") + "\" "
                          "\"9\" \"" + _("termux.mirror_restore") + "\" "
                          "\"A\" \"" + _("termux.mirror_backup_sources") + "\" "
                          "\"B\" \"" + _("termux.mirror_old_format") + "\" "
                          "\"C\" \"" + _("termux.mirror_alpine") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") modify_termux_sources_list("https://mirrors.bfsu.edu.cn/termux/apt");
        else if (choice == "2") modify_termux_sources_list("https://mirrors.cloud.tencent.com/termux/apt");
        else if (choice == "3") modify_termux_sources_list("https://mirrors.tuna.tsinghua.edu.cn/termux/apt");
        else if (choice == "4") modify_termux_sources_list("https://mirrors.ustc.edu.cn/termux/apt");
        else if (choice == "5") manage_termux_repos();
        else if (choice == "6") run_mirror_speed_test();
        else if (choice == "7") edit_sources_manually();
        else if (choice == "8") clean_sources_list();
        else if (choice == "9") restore_default_sources();
        else if (choice == "A" || choice == "a") backup_sources_list();
        else if (choice == "B" || choice == "b") use_old_mirror_format("https://mirrors.tuna.tsinghua.edu.cn/termux");
        else if (choice == "C" || choice == "c") switch_alpine_mirror();
        else return false;

        return true;
    }

    void TermuxManager::modify_termux_sources_list(const std::string &mirror_url) {
        Logger::step(_f("termux.switching_mirror", mirror_url));

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string apt_dir = prefix + "/etc/apt";
        std::string main_list = apt_dir + "/sources.list";
        std::string list_d_dir = apt_dir + "/sources.list.d";

        fs::create_directories(list_d_dir);

        // 注释旧源 + 写入主源 (termux-main)
        {
            std::string new_line = "deb " + mirror_url + "/termux-main stable main";
            annotate_old_list(main_list, new_line);
        }

        // 写入各仓库源
        struct RepoEntry {
            std::string name;
            std::string component;
        };
        std::vector<RepoEntry> repos = {
            {"root", "root stable"},
            {"x11", "x11 main"},
            {"game", "games stable"},
            {"science", "science stable"},
            {"unstable", "unstable main"},
        };

        for (auto &r: repos) {
            std::string repo_file = list_d_dir + "/" + r.name + ".list";
            std::string new_line = "deb " + mirror_url + "/termux-" + r.name + " " + r.component;
            annotate_old_list(repo_file, new_line);
        }

        // 更新包列表
        Logger::step(_("termux.updating_pkg_db"));
        PackageManager::update(DistroFamily::Debian);
        Logger::ok(_("termux.mirror_switch_ok"));
    }

    void TermuxManager::annotate_old_list(const std::string &file_path, const std::string &new_source_line) {
        // 如果文件不存在，直接创建
        if (!fs::exists(file_path)) {
            std::ofstream ofs(file_path);
            if (ofs.is_open()) {
                ofs << new_source_line << "\n";
                ofs.close();
            }
            return;
        }

        // 备份旧文件
        std::string backup = file_path + ".bak";
        if (!fs::exists(backup)) {
            fs::copy_file(file_path, backup);
        }

        // 注释旧源
        Executor::shell("sudo sed -i 's/^\\(deb .*\\)/# \\1/' " + file_path);

        // 追加新源（如果还未添加）
        std::string grep_cmd = "grep -qF '" + new_source_line + "' " + file_path + " 2>/dev/null";
        if (!Executor::shell(grep_cmd).ok()) {
            std::ofstream ofs(file_path, std::ios::app);
            if (ofs.is_open()) {
                ofs << new_source_line << "\n";
                ofs.close();
            }
        }
    }

    void TermuxManager::manage_termux_repos() {
        if (!cfg_.is_termux) return;

        Logger::step(_("termux.repo_management"));

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.repo_title") + "\" "
                          "--menu \"" + _("termux.repo_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.repo_root") + "\" "
                          "\"2\" \"" + _("termux.repo_x11") + "\" "
                          "\"3\" \"" + _("termux.repo_game") + "\" "
                          "\"4\" \"" + _("termux.repo_science") + "\" "
                          "\"5\" \"" + _("termux.repo_unstable") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);
        if (choice == "0" || choice.empty()) return;

        std::string repos[] = {"", "root", "x11", "game", "science", "unstable"};
        int idx = std::stoi(choice);
        if (idx < 1 || idx > 5) return;

        std::string repo = repos[idx];
        std::string action = cfg_.tui_bin + " --title \"" + _f("termux.repo_toggle_title", repo) + "\" "
                             "--yesno \"" + _f("termux.repo_toggle_confirm", repo) + "\" 0 50 "
                             "--yes-button \"" + _("termux.btn_enable") + "\" --no-button \"" + _("termux.btn_disable")
                             + "\"";

        if (Executor::shell(action).ok()) {
            Logger::step(_f("termux.enabling_repo", repo));
            PackageManager::update(DistroFamily::Debian);
            PackageManager::install(repo + "-repo", DistroFamily::Debian);
            PackageManager::update(DistroFamily::Debian);
            Logger::ok(_f("termux.repo_enabled", repo));
        } else {
            Logger::step(_f("termux.disabling_repo", repo));
            PackageManager::remove(repo + "-repo", DistroFamily::Debian);
            PackageManager::update(DistroFamily::Debian);
            Logger::ok(_f("termux.repo_disabled", repo));
        }
    }

    void TermuxManager::run_mirror_speed_test() {
        if (!cfg_.is_termux) return;

        Logger::step(_("termux.speedtest_title"));

        // 确保 aria2c 可用
        if (!Executor::has("aria2c")) {
            Logger::step(_("termux.installing_aria2"));
            PackageManager::install("aria2", DistroFamily::Debian);
        }

        struct MirrorTest {
            std::string name;
            std::string url;
        };

        std::vector<MirrorTest> mirrors = {
            {"BFSU (北外)", "https://mirrors.bfsu.edu.cn/termux/apt/termux-main/pool/main/c/clang/"},
            {"Tencent (腾讯云)", "https://mirrors.cloud.tencent.com/termux/apt/termux-main/pool/main/c/clang/"},
            {"Tsinghua (清华)", "https://mirrors.tuna.tsinghua.edu.cn/termux/apt/termux-main/pool/main/c/clang/"},
            {"USTC (中科大)", "https://mirrors.ustc.edu.cn/termux/apt/termux-main/pool/main/c/clang/"},
            {"Official (官方)", "https://packages.termux.dev/apt/termux-main/pool/main/c/clang/"},
        };

        Logger::info(_("termux.speedtest_header_speed"));
        Logger::info(_("termux.speedtest_separator"));

        std::string temp_dir = "/data/data/com.termux/files/home/.tmoe_speed_test";
        fs::create_directories(temp_dir);

        for (auto &m: mirrors) {
            // 获取最新 clang .deb 文件名
            auto deb_name_result = Executor::shell(
                "curl -sL " + m.url + " | grep -oP 'clang_[^\"]*\\.deb' | head -1");

            std::string deb_name = deb_name_result.stdout_data;
            while (!deb_name.empty() && (deb_name.back() == '\n' || deb_name.back() == '\r')) deb_name.pop_back();

            if (deb_name.empty()) {
                Logger::info(_f("termux.speedtest_fetch_failed", std::string(m.name)));
                continue;
            }

            std::string full_url = m.url + deb_name;

            // 使用 aria2c 下载测速
            auto speed = Executor::shell(
                "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                "--max-connection-per-server=1 --split=1 "
                "--dir=" + temp_dir + " "
                "--out=\".speed_test_" + m.name + "\" "
                "\"" + full_url + "\" 2>&1 | grep -oP 'DL:\\K[0-9.]+[KM]?iB/s' | tail -1");

            std::string speed_str = speed.stdout_data;
            while (!speed_str.empty() && (speed_str.back() == '\n' || speed_str.back() == '\r')) speed_str.pop_back();

            if (speed_str.empty()) speed_str = "N/A";
            Logger::info(std::string(m.name) + "  " + speed_str);
        }

        // 清理
        fs::remove_all(temp_dir);
        Logger::ok(_("termux.speedtest_complete"));
    }

    void TermuxManager::edit_sources_manually() {
        if (!cfg_.is_termux) return;

        std::string editor = "nano";
        if (Executor::has("vim")) editor = "vim";
        if (Executor::has("micro")) editor = "micro";

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string sources = prefix + "/etc/apt/sources.list";

        Logger::step(_f("termux.editing_sources", editor, sources));
        Executor::shell(editor + " " + sources);
    }

    void TermuxManager::clean_sources_list() {
        if (!cfg_.is_termux) return;

        Logger::step(_("termux.cleaning_sources"));

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string apt_dir = prefix + "/etc/apt";

        // 处理主 sources.list
        std::string main_file = apt_dir + "/sources.list";
        if (fs::exists(main_file)) {
            Executor::shell("sudo sed -i '/^#/d' " + main_file);
            Executor::shell("sort -u -o " + main_file + " " + main_file);
        }

        // 处理 sources.list.d/*.list
        std::string list_d = apt_dir + "/sources.list.d";
        if (fs::exists(list_d)) {
            for (auto &entry: fs::directory_iterator(list_d)) {
                if (entry.path().extension() == ".list") {
                    std::string p = entry.path().string();
                    Executor::shell("sudo sed -i '/^#/d' " + p);
                    Executor::shell("sort -u -o " + p + " " + p);
                }
            }
        }

        Logger::ok(_("termux.sources_cleaned"));
    }

    void TermuxManager::restore_default_sources() {
        if (!cfg_.is_termux) return;

        std::string confirm = cfg_.tui_bin + " --title \"" + _("termux.restore_default_title") + "\" "
                              "--yesno \"" + _("termux.restore_default_confirm") + "\" 0 50";
        if (!Executor::shell(confirm).ok()) return;

        Logger::step(_("termux.restoring_default_sources"));

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string main_file = prefix + "/etc/apt/sources.list";
        std::string list_d = prefix + "/etc/apt/sources.list.d";

        // 写入默认源
        {
            std::ofstream ofs(main_file);
            if (ofs.is_open()) {
                ofs << "deb https://packages.termux.dev/apt/termux-main stable main\n";
                ofs.close();
            }
        }

        // 清理 sources.list.d 中的所有自定义源文件
        if (fs::exists(list_d)) {
            for (auto &entry: fs::directory_iterator(list_d)) {
                if (entry.path().extension() == ".list") {
                    std::string p = entry.path().string();
                    std::string bak = p + ".bak";
                    if (!fs::exists(bak)) {
                        fs::copy_file(p, bak);
                    }
                    fs::remove(p);
                }
            }
        }

        Logger::ok(_("termux.restore_defaults_ok"));
        PackageManager::update(DistroFamily::Debian);
    }

    // ═══════════════════════════════════════════════════════════════
    // Android 12+ Signal 9 修复
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::fix_android_12_signal_9() {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.skip_signal9"));
            return true;
        }

        Logger::step(_("termux.signal9_wizard_title"));

        // 扩展菜单
        std::string ask = cfg_.tui_bin + " --title \"" + _("termux.signal9_detect_title") + "\" "
                          "--yesno \"" + _("termux.signal9_detect_yesno") + "\" 0 50";
        if (!Executor::shell(ask).ok()) {
            Logger::info(_("termux.signal9_skip"));
            return true;
        }

        // 增强菜单
        std::string fix_menu = cfg_.tui_bin + " --title \"" + _("termux.signal9_fix_title") + "\" "
                               "--menu \"" + _("termux.signal9_fix_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("termux.signal9_adb_fix") + "\" "
                               "\"2\" \"" + _("termux.signal9_samsung") + "\" "
                               "\"3\" \"" + _("termux.signal9_adb_pair") + "\" "
                               "\"4\" \"" + _("termux.signal9_adb_port") + "\" "
                               "\"5\" \"" + _("termux.signal9_verify") + "\" "
                               "\"6\" \"" + _("termux.signal9_manual") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(fix_menu);

        if (choice == "1") {
            return connect_adb_and_fix();
        } else if (choice == "2") {
            set_samsung_adb_comp_mode();
            return connect_adb_and_fix();
        } else if (choice == "3") {
            if (adb_pair_and_connect_flow()) {
                return execute_max_phantom_fix("");
            }
        } else if (choice == "4") {
            select_adb_port();
        } else if (choice == "5") {
            verify_signal9_fix();
        } else if (choice == "6") {
            Logger::info(_("termux.signal9_manual_cmd_title"));
            Logger::info(
                "adb shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\"");
            Logger::info("adb shell \"/system/bin/settings put global settings_enable_monitor_phantom_procs false\"");
            Logger::info("adb shell \"/system/bin/device_config set_sync_disabled_for_tests persistent\"");
        }

        return true;
    }

    bool TermuxManager::connect_adb_and_fix() {
        // 1. 确保 adb 已安装 (Termux: android-tools, GNU/Linux: adb)
        if (!Executor::has("adb")) {
            Logger::step(_("termux.installing_adb"));
            if (cfg_.is_termux) {
                PackageManager::update(DistroFamily::Debian);
                PackageManager::install("android-tools", DistroFamily::Debian);
            } else {
                Executor::shell("apt-get install -y adb 2>/dev/null || apt install -y adb 2>/dev/null || true");
            }
            if (!Executor::has("adb")) {
                Logger::error(_("termux.adb_install_failed"));
                return false;
            }
        }

        // 2. 三星设备处理
        if (is_samsung_device()) {
            Logger::warn(_("termux.samsung_detected"));
            std::string samsung_ask = cfg_.tui_bin + " --title \"" + _("termux.samsung_title") + "\" "
                                      "--yesno \"" + _("termux.samsung_yesno") + "\" 0 50";
            if (Executor::shell(samsung_ask).ok()) {
                set_samsung_adb_comp_mode();
            }
        }

        // 3. 连接方式选择
        std::string method = cfg_.tui_bin + " --title \"" + _("termux.adb_connect_title") + "\" "
                             "--menu \"" + _("termux.adb_connect_prompt") + "\" 0 50 0 "
                             "\"1\" \"" + _("termux.adb_wireless_pair") + "\" "
                             "\"2\" \"" + _("termux.adb_direct") + "\" "
                             "\"3\" \"" + _("termux.adb_usb") + "\" "
                             "\"4\" \"" + _("termux.adb_list_devices") + "\" "
                             "\"0\" \"" + _("termux.adb_skip") + "\"";

        auto choice = Executor::tui_select(method);
        std::string adb_target;

        if (choice == "1") {
            adb_pair_and_connect_flow();
            // 自动检测设备
            int count = count_adb_devices();
            if (count <= 0) {
                Logger::warn(_("termux.adb_no_device"));
            }
        } else if (choice == "2") {
            std::string conn_cmd = cfg_.tui_bin + " --title \"" + _("termux.adb_addr_title") + "\" "
                                   "--inputbox \"" + _("termux.adb_addr_input") + "\" 0 50";
            adb_target = Executor::tui_select(conn_cmd);
            if (!adb_target.empty()) {
                Logger::step(_f("termux.adb_connecting", adb_target));
                Executor::shell("adb connect " + adb_target);
            }
        } else if (choice == "3") {
            Logger::info(_("termux.adb_usb_hint"));
        } else if (choice == "4") {
            Executor::shell("adb devices -l");
            Logger::info(_("termux.adb_check_device_status"));
        } else {
            Logger::info(_("termux.signal9_manual_cmd_title"));
            Logger::info(
                "adb shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\"");
            Logger::info("adb shell \"/system/bin/settings put global settings_enable_monitor_phantom_procs false\"");
            return true;
        }

        // 4. 执行修复
        return execute_max_phantom_fix(adb_target);
    }

    bool TermuxManager::is_samsung_device() {
        auto result = Executor::shell("getprop ro.product.manufacturer 2>/dev/null");
        std::string mfr = result.stdout_data;
        std::transform(mfr.begin(), mfr.end(), mfr.begin(), ::tolower);
        // 也检查 ro.product.brand 和 ro.product.vendor.manufacturer
        if (mfr.find("samsung") == std::string::npos) {
            result = Executor::shell("getprop ro.product.brand 2>/dev/null");
            mfr = result.stdout_data;
            std::transform(mfr.begin(), mfr.end(), mfr.begin(), ::tolower);
        }
        return mfr.find("samsung") != std::string::npos;
    }

    bool TermuxManager::execute_max_phantom_fix(const std::string &adb_target) {
        Logger::step(_("termux.signal9_executing_fix"));

        std::string adb_prefix = "adb";
        if (!adb_target.empty()) {
            adb_prefix += " -s " + adb_target;
        }

        // 检查设备连接
        auto devices = Executor::shell("adb devices -l 2>/dev/null");
        if (devices.stdout_data.find("device") == std::string::npos) {
            Logger::error(_("termux.signal9_no_adb_device"));
            Logger::info(_("termux.signal9_manual_cmd_hint"));
            Logger::info("adb shell \"device_config put activity_manager max_phantom_processes 2147483647\"");
            return false;
        }

        // 修复前验证 (if possible)
        auto before = Executor::shell(
            adb_prefix + " shell dumpsys activity settings 2>/dev/null | grep max_phantom_processes");
        if (before.ok() && !before.stdout_data.empty()) {
            Logger::info(_f("termux.signal9_before_state", before.stdout_data));
        }

        // 命令 1: 设置最大 phantom 进程数为 2147483647
        Logger::step(_("termux.signal9_set_max_phantom"));
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\" 2>/dev/null; true");

        // 命令 2: 禁用 phantom 进程监控
        Logger::step(_("termux.signal9_disable_monitor"));
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/settings put global settings_enable_monitor_phantom_procs false\" 2>/dev/null; true");

        // 命令 3: 持久化 (set_sync_disabled_for_tests)
        Logger::step(_("termux.signal9_persist_settings"));
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/device_config set_sync_disabled_for_tests persistent\" 2>/dev/null; true");

        // 修复后验证
        auto after = Executor::shell(
            adb_prefix + " shell dumpsys activity settings 2>/dev/null | grep max_phantom_processes");
        if (after.ok() && !after.stdout_data.empty()) {
            Logger::ok(_f("termux.signal9_after_state", after.stdout_data));
        } else {
            Logger::warn(_("termux.signal9_cannot_verify"));
        }

        Logger::ok(_("termux.signal9_fix_executed"));
        Logger::info(_("termux.signal9_reboot_hint"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 磁盘空间占用
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::check_disk_usage() {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.disk_non_termux"));
            return;
        }

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.disk_title") + "\" "
                          "--menu \"" + _("termux.disk_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.disk_dir_ranking") + "\" "
                          "\"2\" \"" + _("termux.disk_large_files") + "\" "
                          "\"3\" \"" + _("termux.disk_sdcard") + "\" "
                          "\"4\" \"" + _("termux.disk_overall") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") show_termux_dir_usage();
        else if (choice == "2") show_termux_large_files();
        else if (choice == "3") show_sdcard_usage();
        else if (choice == "4") show_overall_disk_usage();
    }

    void TermuxManager::show_termux_dir_usage() {
        Logger::step(_("termux.disk_dir_ranking_title"));

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";

        Logger::info(_("termux.disk_home_top15"));
        Executor::shell("cd " + home + " && du -hsx ./* ./.* 2>/dev/null | sort -rh | head -15");

        Logger::info(_("termux.disk_usr_top6"));
        Executor::shell("cd " + prefix + " && du -hsx ./* 2>/dev/null | sort -rh | head -6");

        Logger::info(_("termux.disk_usr_lib_top8"));
        std::string usr_lib = prefix + "/lib";
        if (fs::exists(usr_lib)) {
            Executor::shell("du -hsx " + usr_lib + "/* 2>/dev/null | sort -rh | head -8");
        }

        Logger::info(_("termux.disk_usr_share_top8"));
        std::string usr_share = prefix + "/share";
        if (fs::exists(usr_share)) {
            Executor::shell("du -hsx " + usr_share + "/* 2>/dev/null | sort -rh | head -8");
        }

        Logger::ok(_("termux.disk_dir_query_done"));
    }

    void TermuxManager::show_termux_large_files() {
        Logger::step(_("termux.disk_large_files_title"));

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files";
        Executor::shell(
            "cd " + prefix +
            " && find ./ -type f -print0 2>/dev/null | xargs -0 du 2>/dev/null | sort -n | tail -30 | cut -f2 | xargs -I{} du -sh {} 2>/dev/null");

        Logger::ok(_("termux.disk_file_query_done"));
    }

    void TermuxManager::show_sdcard_usage() {
        Logger::step(_("termux.disk_sdcard_title"));

        if (!fs::exists("/sdcard")) {
            Logger::warn(_("termux.disk_sdcard_not_found"));
            return;
        }

        Logger::info(_("termux.disk_sdcard_dir_top15"));
        Executor::shell("cd /sdcard && du -hsx ./* ./.* 2>/dev/null | sort -rh | head -15");

        Logger::info(_("termux.disk_sdcard_file_top30"));
        Executor::shell(
            "cd /sdcard && find ./ -type f -print0 2>/dev/null | xargs -0 du 2>/dev/null | sort -n | tail -30 | cut -f2 | xargs -I{} du -sh {} 2>/dev/null");

        Logger::ok(_("termux.disk_sdcard_query_done"));
    }

    void TermuxManager::show_overall_disk_usage() {
        Logger::step(_("termux.disk_overall_title"));
        Executor::shell("df -h | grep G | grep -v tmpfs");
        Logger::ok(_("termux.disk_query_done"));
    }

    // ═══════════════════════════════════════════════════════════════
    // TUI 兼容补丁 (已完成)
    // ═══════════════════════════════════════════════════════════════

    std::string TermuxManager::check_and_patch_tui_env() {
        if (!cfg_.is_termux) return "whiptail";

        if (!Executor::has("whiptail")) {
            Logger::step(_("termux.missing_whiptail"));
            PackageManager::update(DistroFamily::Debian);
            PackageManager::install({"whiptail", "dialog"}, DistroFamily::Debian);
        }

        auto dpkg_query = Executor::shell("LANG=C dpkg-query -W libnewt 2>/dev/null");
        if (dpkg_query.stdout_data.find("0.52.21") != std::string::npos) {
            Logger::warn(_("termux.tui_libnewt_buggy"));

            std::string lib_popt = cfg_.work_dir.string() + "/usr/lib/popt0/0.so";
            std::string wrapper = cfg_.work_dir.string() + "/usr/bin/whiptail-wrapper";
            std::string bin_dir = cfg_.work_dir.string() + "/usr/bin";

            if (!std::filesystem::exists(lib_popt)) {
                Logger::step(_("termux.tui_fetching_libpopt"));
                std::string arch = cfg_.arch;
                std::string uri = "https://packages.tmoe.me/patch/termux/libp/libpopt0_1.18_" + arch + ".tar.gz";
                std::string tar_file = cfg_.temp_dir.string() + "/libpopt.tar.gz";

                CommandBuilder("curl").add_flag("-Lo").add_arg(tar_file).add_arg(uri).execute();
                CommandBuilder("tar").add_flag("-zxvf").add_arg(tar_file).add_arg("-C").add_arg(cfg_.work_dir.string()).execute();
                CommandBuilder("rm").add_flag("-f").add_arg(tar_file).execute();
            }

            if (std::filesystem::exists(lib_popt)) {
                std::filesystem::create_directories(bin_dir);
                std::ofstream ofs(wrapper);
                if (ofs.is_open()) {
                    ofs << "#!/usr/bin/env bash\n";
                    ofs << "env LD_PRELOAD=" << lib_popt << " whiptail \"$@\"\n";
                    ofs.close();
                    CommandBuilder("chmod").add_flag("+x").add_arg(wrapper).execute();
                    Logger::ok(_("termux.tui_patch_applied"));
                    return wrapper;
                }
            }
        }
        return "whiptail";
    }

    // ═══════════════════════════════════════════════════════════════
    // 配色/字体/按键 (已完成)
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::termux_color_scheme_menu() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.color_title") + "\" "
                          "--menu \"" + _("termux.color_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.color_neon") + "\" "
                          "\"2\" \"" + _("termux.color_monokai") + "\" "
                          "\"3\" \"" + _("termux.color_material") + "\" "
                          "\"4\" \"" + _("termux.color_bright") + "\" "
                          "\"5\" \"" + _("termux.color_materia") + "\" "
                          "\"6\" \"" + _("termux.color_miu") + "\" "
                          "\"7\" \"" + _("termux.color_wildcherry") + "\" "
                          "\"0\" \"" + _("termux.skip") + "\"";

        std::string choice = Executor::tui_select(cmd);
        std::string color_file;

        if (choice == "1") color_file = "neon";
        else if (choice == "2") color_file = "monokai.dark";
        else if (choice == "3") color_file = "material";
        else if (choice == "4") color_file = "bright.light";
        else if (choice == "5") color_file = "materia";
        else if (choice == "6") color_file = "miu";
        else if (choice == "7") color_file = "wild.cherry";

        if (!color_file.empty()) {
            Logger::step(_f("termux.color_downloading", color_file));
            std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/colors/" + color_file;
            std::string dest = std::string(std::getenv("HOME")) + "/.termux/colors.properties";
            CommandBuilder("curl").add_flag("-L").add_flag("-o").add_arg(dest).add_arg(url).execute();
        }
    }

    void TermuxManager::termux_font_menu() {
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.font_title") + "\" "
                          "--menu \"" + _("termux.font_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.font_inconsolata") + "\" "
                          "\"2\" \"" + _("termux.font_iosevka") + "\" "
                          "\"3\" \"" + _("termux.font_iosevka_bold") + "\" "
                          "\"4\" \"" + _("termux.font_iosevka_mono") + "\" "
                          "\"5\" \"" + _("termux.font_fira") + "\" "
                          "\"6\" \"" + _("termux.font_fira_medium") + "\" "
                          "\"0\" \"" + _("termux.skip") + "\"";

        std::string choice = Executor::tui_select(cmd);
        std::string font_path;

        if (choice == "1") font_path = "inconsolata-go-font/raw/master/inconsolatago.tar.xz";
        else if (choice == "2") font_path = "inconsolata-go-font/raw/master/iosevka.tar.xz";
        else if (choice == "3") font_path = "iosevka-italic-font/raw/master/font.tar.xz";
        else if (choice == "4") font_path = "iosevka-term-mono/raw/master/font.tar.xz";
        else if (choice == "5") font_path = "fira-code/raw/master/font.tar.xz";
        else if (choice == "6") font_path = "fira-code-medium/raw/master/font.tar.xz";

        if (!font_path.empty()) {
            Logger::step(_("termux.font_downloading"));
            std::string termux_dir = std::string(std::getenv("HOME")) + "/.termux";
            std::string url = "https://gitee.com/ak2/" + font_path;
            std::string tar_file = termux_dir + "/font.tar.xz";

            CommandBuilder("curl").add_flag("-L").add_flag("-o").add_arg(tar_file).add_arg(url).execute();
            CommandBuilder("tar").add_flag("-Jxvf").add_arg(tar_file).add_arg("-C").add_arg(termux_dir).execute();
            CommandBuilder("rm").add_flag("-f").add_arg(tar_file).execute();
        }
    }

    void TermuxManager::configure_extra_keys() {
        std::string termux_dir = std::string(std::getenv("HOME")) + "/.termux";
        std::string prop_file = termux_dir + "/termux.properties";

        if (!fs::exists(prop_file) || Executor::shell("grep -q '# extra-keys-style = default' " + prop_file).ok()) {
            std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.keys_title") + "\" "
                              "--yesno \"" + _("termux.keys_yesno") + "\" 10 50";

            if (Executor::shell(cmd).ok()) {
                Logger::step(_("termux.keys_configuring"));
                std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/termux.properties";
                std::string tmp_file = termux_dir + "/termux.properties.02";

                if (fs::exists(prop_file)) CommandBuilder("cp").add_flag("-f").add_arg(prop_file).add_arg(prop_file + ".bak").execute();
                CommandBuilder("curl").add_flag("-L").add_flag("-o").add_arg(tmp_file).add_arg(url).execute();

                Executor::shell(
                    "sed -i -E 's@# (extra-keys-style)@#\\1@g;s@^[^#]@#&@g;1r " + tmp_file + "' " + prop_file);

                if (!fs::exists(prop_file)) {
                    CommandBuilder("mv").add_flag("-f").add_arg(tmp_file).add_arg(prop_file).execute();
                } else {
                    CommandBuilder("rm").add_flag("-f").add_arg(tmp_file).execute();
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 主菜单调度
    // ═══════════════════════════════════════════════════════════════

    int TermuxManager::run_termux_menu() {
        while (true) {
            std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.title") + "\" "
                              "--menu \"" + _("termux.menu_prompt") + "\" 0 50 0 "
                              "\"1\" \"" + _("termux.gui") + "\" "
                              "\"2\" \"" + _("termux.backup") + "\" "
                              "\"3\" \"" + _("termux.restore") + "\" "
                              "\"4\" \"" + _("termux.beautify") + "\" "
                              "\"5\" \"" + _("termux.mirror") + "\" "
                              "\"6\" \"" + _("termux.disk_usage") + "\" "
                              "\"7\" \"" + _("termux.env_init") + "\" "
                              "\"8\" \"" + _("termux.signal9") + "\" "
                              "\"9\" \"" + _("termux.storage") + "\" "
                              "\"A\" \"" + _("termux.pulseaudio") + "\" "
                              "\"B\" \"" + _("termux.self_update") + "\" "
                              "\"0\" \"" + _("menu.tui.back") + "\"";

            auto choice = Executor::tui_select(cmd);

            if (choice == "1") run_termux_gui_menu();
            else if (choice == "2") backup_termux();
            else if (choice == "3") restore_termux();
            else if (choice == "4") beautify_terminal();
            else if (choice == "5") switch_pkg_mirror();
            else if (choice == "6") check_disk_usage();
            else if (choice == "7") check_and_init_environment();
            else if (choice == "8") fix_android_12_signal_9();
            else if (choice == "9") setup_storage();
            else if (choice == "A" || choice == "a") {
                std::string pa_cmd = cfg_.tui_bin + " --title \"" + _("termux.pulseaudio_title") + "\" "
                                     "--menu \"" + _("termux.pulseaudio_prompt") + "\" 0 50 0 "
                                     "\"1\" \"" + _("termux.pa_tcp_local") + "\" "
                                     "\"2\" \"" + _("termux.pa_lan_toggle") + "\" "
                                     "\"3\" \"" + _("termux.pa_idle_timeout") + "\" "
                                     "\"0\" \"" + _("menu.tui.back") + "\"";
                auto pa_choice = Executor::tui_select(pa_cmd);
                if (pa_choice == "1") configure_pulseaudio_tcp();
                else if (pa_choice == "2") toggle_lan_audio();
                else if (pa_choice == "3") configure_pulseaudio_idle_timeout();
            } else if (choice == "B" || choice == "b") self_update();
            else if (choice == "0" || choice.empty()) return 0;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // PulseAudio 配置
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::configure_pulseaudio_tcp() {
        if (!cfg_.is_termux) {
            Logger::info(_("termux.pa_skip_non_termux"));
            return true;
        }

        Logger::step(_("termux.pa_configuring_tcp"));

        std::string pa_conf = "/data/data/com.termux/files/usr/etc/pulse/default.pa";
        if (!fs::exists(pa_conf)) {
            Logger::warn(_f("termux.pa_conf_missing", pa_conf));
            Logger::info(_("termux.installing_pulseaudio"));
            PackageManager::install("pulseaudio", DistroFamily::Debian);
            if (!fs::exists(pa_conf)) {
                Executor::shell("pulseaudio --start 2>/dev/null; sleep 1; killall pulseaudio 2>/dev/null");
            }
        }

        if (!fs::exists(pa_conf)) {
            Logger::error(_("termux.pa_conf_not_found"));
            return false;
        }

        // 检测是否已配置 TCP 原生协议
        if (Executor::shell("grep -Eq '^[^#]*load-module module-native-protocol-tcp' " + pa_conf).ok()) {
            Logger::info(_("termux.pa_tcp_already_configured"));
            return true;
        }

        // 清理旧配置并添加新配置
        Executor::shell("sudo sed -i '/auth-ip-acl/d;/module-native-protocol-tcp/d' " + pa_conf);
        Executor::shell(
            "echo 'load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1' >> " + pa_conf);

        Logger::ok(_("termux.pa_tcp_configured"));
        return true;
    }

    bool TermuxManager::configure_pulseaudio_idle_timeout() {
        if (!cfg_.is_termux) return true;

        Logger::step(_("termux.pa_idle_timeout_configuring"));

        std::string daemon_conf = "/data/data/com.termux/files/usr/etc/pulse/daemon.conf";
        if (!fs::exists(daemon_conf)) {
            Logger::warn(_("termux.pa_daemon_conf_missing"));
            return false;
        }

        // 设置 exit-idle-time = 3600 (1小时)
        if (Executor::shell("grep -q 'exit-idle-time' " + daemon_conf).ok()) {
            Executor::shell("sudo sed -i 's/^[#; ]*exit-idle-time.*$/exit-idle-time = 3600/' " + daemon_conf);
        } else {
            Executor::shell("echo 'exit-idle-time = 3600' >> " + daemon_conf);
        }

        Logger::ok(_("termux.pa_idle_timeout_set"));
        return true;
    }

    bool TermuxManager::toggle_lan_audio() {
        if (!cfg_.is_termux) return true;

        Logger::step(_("termux.pa_lan_toggle_configuring"));

        std::string pa_conf = "/data/data/com.termux/files/usr/etc/pulse/default.pa";
        if (!fs::exists(pa_conf)) {
            Logger::error(_("termux.pa_default_pa_missing"));
            return false;
        }

        // 检测当前局域网音频状态
        bool lan_enabled = Executor::shell(
            "grep -Eq '192\\.168\\.0\\.0/16.*172\\.16\\.0\\.0/12' " + pa_conf).ok();

        std::string status_msg = lan_enabled ? _("termux.lan_audio_enabled") : _("termux.lan_audio_local_only");
        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.lan_audio_title") + "\" "
                          "--yesno \"" + _f("termux.lan_audio_yesno", status_msg) + "\" 12 60";

        if (!Executor::shell(cmd).ok()) {
            Logger::info(_("termux.pa_lan_cancelled"));
            return false;
        }

        // 移除旧的 auth-ip-acl 行
        Executor::shell("sudo sed -i '/auth-ip-acl/d' " + pa_conf);

        if (lan_enabled) {
            // 禁用局域网, 仅限 localhost
            Executor::shell(
                "echo 'load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=0' >> " + pa_conf);
            Logger::ok(_("termux.pa_lan_disabled"));
        } else {
            // 启用局域网
            Executor::shell(
                "echo 'load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1;192.168.0.0/16;172.16.0.0/12 auth-anonymous=1' >> "
                + pa_conf);
            Logger::ok(_("termux.pa_lan_enabled"));
        }

        Logger::warn(_("termux.pa_restart_hint"));
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 自更新
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::self_update() {
        Logger::step(_("termux.self_update_title"));

        std::string git_dir = cfg_.work_dir.string();
        if (!fs::exists(git_dir + "/.git")) {
            Logger::error(_f("termux.self_update_no_git", git_dir));
            Logger::info(_("termux.self_update_git_hint"));
            return false;
        }

        Logger::step(_("termux.self_update_pulling"));
        Executor::shell("cd " + git_dir + " && git reset --hard origin/master 2>/dev/null || true");

        auto result = Executor::shell(
            "cd " + git_dir + " && git pull --rebase --stat origin master --allow-unrelated-histories 2>&1");

        if (!result.ok()) {
            Logger::warn(_("termux.self_update_rebase_failed"));
            Executor::shell("cd " + git_dir + " && git rebase --skip 2>/dev/null || true");
            result = Executor::shell(
                "cd " + git_dir + " && git pull --rebase --stat origin master --allow-unrelated-histories 2>&1");
        }

        // termux-fix-shebang 修复脚本
        if (cfg_.is_termux) {
            Logger::step(_("termux.self_update_fix_shebang"));
            std::vector<std::string> fix_targets = {"tmoe", "debian-i", "lnk-menu", "debian"};
            if (Executor::has("termux-fix-shebang")) {
                for (auto &t: fix_targets) {
                    std::string bin_path = "/data/data/com.termux/files/usr/bin/" + t;
                    if (fs::exists(bin_path)) {
                        CommandBuilder("termux-fix-shebang").add_arg(bin_path).add_raw("2>/dev/null").execute();
                    }
                }
            }

            // 创建 tome 别名
            if (fs::exists("/data/data/com.termux/files/usr/bin/tmoe")) {
                CommandBuilder("ln").add_flag("-sf").add_arg("tmoe").add_arg("/data/data/com.termux/files/usr/bin/tome").add_raw("2>/dev/null").execute();
            }

            // 修复 debian-i 符号链接
            if (fs::exists(cfg_.work_dir / "share/manager")) {
                CommandBuilder("ln").add_flag("-sf").add_arg((cfg_.work_dir / "share/manager").string()).add_arg("/data/data/com.termux/files/usr/bin/debian-i").add_raw("2>/dev/null").execute();
            }
        }

        // 更新后问候语
        Logger::ok(_("termux.self_update_done"));

        // Try fortune/hitokoto greeting
        if (Executor::has("fortune")) {
            auto fortune = Executor::shell("fortune 2>/dev/null");
            if (fortune.ok() && !fortune.stdout_data.empty()) {
                Logger::info(fortune.stdout_data);
            }
        }

        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 镜像源增强
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::write_termux_source(const std::string &file_path, const std::string &repo_name,
                                            const std::string &mirror_url, const std::string &component) {
        std::string deb_line = "deb " + mirror_url + "/" + repo_name + "-" + component;
        std::ofstream ofs(file_path);
        if (ofs.is_open()) {
            ofs << deb_line << "\n";
            ofs.close();
        }
    }

    void TermuxManager::use_old_mirror_format(const std::string &mirror_url) {
        Logger::step(_("termux.mirror_old_format_configuring"));

        std::string sources_dir = "/data/data/com.termux/files/usr/etc/apt/sources.list.d";
        fs::create_directories(sources_dir);

        write_termux_source(sources_dir + "/main.list", "termux-packages", mirror_url, "24 stable main");
        write_termux_source(sources_dir + "/game.list", "game-packages", mirror_url, "24 games stable");
        write_termux_source(sources_dir + "/science.list", "science-packages", mirror_url, "24 science stable");

        Logger::ok(_("termux.mirror_old_format_done"));
    }

    bool TermuxManager::check_android_version_for_mirror() {
        if (!cfg_.is_termux) return true;

        // 检测 Android 版本
        auto result = Executor::shell("getprop ro.build.version.release 2>/dev/null");
        if (!result.ok()) {
            // 无法检测，假设是新版本
            return true;
        }

        std::string version = result.stdout_data;
        while (!version.empty() && !std::isdigit(version[0])) version.erase(0, 1);
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();

        if (version.empty()) return true;

        try {
            int major = std::stoi(version);
            if (major < 7) {
                Logger::error(_f("termux.android_old_version", version));
                Logger::error(_("termux.android_old_format_hint"));
                std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.android_version_title") + "\" "
                                  "--yesno \"" + _f("termux.android_version_yesno", version) + "\" 10 50 "
                                  "--yes-button \"" + _("termux.android_use_old") + "\" --no-button \"" + _(
                                      "termux.btn_cancel") + "\"";

                if (Executor::shell(cmd).ok()) {
                    use_old_mirror_format("https://mirrors.tuna.tsinghua.edu.cn/termux");
                    return true;
                }
                return false;
            }
        } catch (...) {
            // 解析失败，继续
        }

        return true;
    }

    void TermuxManager::backup_sources_list() {
        Logger::step(_("termux.mirror_backup_sources_list"));

        std::string backup_file = "/sdcard/Download/backup/sources-list_bak.tar.xz";
        fs::create_directories("/sdcard/Download/backup");

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string apt_dir = prefix + "/etc/apt";
        std::string sources_list = apt_dir + "/sources.list";
        std::string sources_list_d = apt_dir + "/sources.list.d";

        std::string tar_targets;
        if (fs::exists(sources_list)) tar_targets += " " + sources_list;
        if (fs::exists(sources_list_d)) tar_targets += " " + sources_list_d;

        if (tar_targets.empty()) {
            Logger::warn(_("termux.mirror_no_sources_found"));
            return;
        }

        CommandBuilder("tar").add_flag("-PJcvf").add_arg(backup_file).add_raw(tar_targets).execute();
        Logger::ok(_f("termux.mirror_backup_done", backup_file));
    }

    void TermuxManager::switch_alpine_mirror() {
        Logger::step(_("termux.alpine_mirror_switching"));

        std::string apk_repos = "/etc/apk/repositories";
        if (!fs::exists(apk_repos)) {
            Logger::info(_("termux.alpine_mirror_skip"));
            return;
        }

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.alpine_mirror_title") + "\" "
                          "--menu \"" + _("termux.alpine_mirror_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.alpine_tuna") + "\" "
                          "\"2\" \"" + _("termux.alpine_official") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") {
            // 备份原文件
            CommandBuilder("cp").add_arg(apk_repos).add_arg(apk_repos + ".bak").add_raw("2>/dev/null").execute();
            Executor::shell(
                "sed -i 's|http[s]*://[^/]*/alpine|https://mirrors.tuna.tsinghua.edu.cn/alpine|g' " + apk_repos);
            Logger::ok(_("termux.alpine_mirror_switched_tuna"));
        } else if (choice == "2") {
            Executor::shell("sudo sed -i 's|http[s]*://[^/]*/alpine|https://dl-cdn.alpinelinux.org/alpine|g' " + apk_repos);
            Logger::ok(_("termux.alpine_mirror_restored_official"));
        }
    }

    void TermuxManager::linux_mirror_fallback() {
        if (cfg_.is_termux) {
            // Termux 环境使用自己的镜像管理
            return;
        }

        Logger::info(_("termux.linux_mirror_fallback"));

        // 检测发行版并为不同的包管理器配置镜像
        std::string mirror_url = "https://mirrors.tuna.tsinghua.edu.cn";

        // Debian/Ubuntu
        if (fs::exists("/etc/apt/sources.list")) {
            std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.linux_mirror_title") + "\" "
                              "--menu \"" + _("termux.linux_mirror_prompt") + "\" 0 50 0 "
                              "\"1\" \"" + _("termux.linux_mirror_tuna") + "\" "
                              "\"2\" \"" + _("termux.linux_mirror_restore") + "\" "
                              "\"3\" \"" + _("termux.linux_mirror_backup") + "\" "
                              "\"0\" \"" + _("menu.tui.back") + "\"";

            auto choice = Executor::tui_select(cmd);

            if (choice == "1") {
                CommandBuilder("cp").add_arg("/etc/apt/sources.list").add_arg("/etc/apt/sources.list.bak").add_raw("2>/dev/null").execute();
                Executor::shell("sudo sed -i 's|http[s]*://[^/]*/|" + mirror_url + "/|g' /etc/apt/sources.list");
                Logger::ok(_("termux.linux_mirror_switched_tuna"));
            } else if (choice == "2") {
                if (fs::exists("/etc/apt/sources.list.bak")) {
                    Executor::shell("sudo cp /etc/apt/sources.list.bak /etc/apt/sources.list");
                    Logger::ok(_("termux.linux_mirror_restored"));
                } else {
                    Logger::warn(_("termux.linux_mirror_no_backup"));
                }
            } else if (choice == "3") {
                Executor::shell(
                    "tar -PJcvf /tmp/sources-list_deb_bak.tar.xz /etc/apt/sources.list /etc/apt/sources.list.d 2>/dev/null");
                Logger::ok(_f("termux.linux_mirror_backup_done", "/tmp/sources-list_deb_bak.tar.xz"));
            }
        }

        // Alpine
        if (fs::exists("/etc/apk/repositories")) {
            switch_alpine_mirror();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // Signal 9 修复增强
    // ═══════════════════════════════════════════════════════════════

    std::string TermuxManager::run_adb_cmd(const std::string &cmd) {
        auto result = Executor::shell(cmd);
        if (!result.ok()) {
            Logger::warn(_f("termux.adb_cmd_failed", result.stderr_data));
        }
        return result.stdout_data;
    }

    bool TermuxManager::set_samsung_adb_comp_mode() {
        if (!is_samsung_device()) return false;

        Logger::step(_("termux.samsung_adb_comp_enabling"));

        // 三星设备存在 smart socket 兼容性问题
        // 使用 Unix 域套接字 + fakeroot + FWMARK 标志解决
        std::string tmpdir = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/data/data/com.termux/files/usr/tmp";

        // 清理旧套接字
        std::string sock_file = tmpdir + "/adb.sock";
        if (fs::exists(sock_file)) {
            fs::remove(sock_file);
            Logger::info(_f("termux.samsung_adb_sock_cleaned", sock_file));
        }

        // 设置环境变量
        Executor::shell("export ADB_SERVER_SOCKET=localfilesystem:" + sock_file);
        Executor::shell("export ANDROID_NO_USE_FWMARK_CLIENT=1");

        // 使用 fakeroot 重启 ADB 服务端
        if (Executor::has("fakeroot")) {
            Logger::step(_("termux.samsung_adb_fakeroot_restart"));
            Executor::shell("pkill adb 2>/dev/null; fakeroot adb kill-server 2>/dev/null || adb kill-server");
            Executor::shell("fakeroot adb start-server 2>/dev/null || adb start-server");
        } else {
            Logger::warn(_("termux.samsung_adb_install_fakeroot_hint"));
            Executor::shell("adb kill-server && adb start-server");
        }

        Logger::ok(_("termux.samsung_adb_comp_enabled"));
        return true;
    }

    bool TermuxManager::adb_pair_and_connect_flow() {
        Logger::step(_("termux.adb_pair_connect_starting"));

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.adb_wireless_title") + "\" "
                          "--inputbox \"" + _("termux.adb_wireless_input") + R"(" 0 50 "")";

        std::string addr = Executor::tui_select(cmd);
        if (addr.empty()) {
            Logger::info(_("termux.adb_pair_cancelled"));
            return false;
        }

        // 阶段 1: 配对
        std::string pair_cmd = cfg_.tui_bin + " --title \"" + _("termux.adb_pair_title") + "\" "
                               "--inputbox \"" + _f("termux.adb_pair_input", addr) + R"(" 0 50 "")";

        std::string pair_code = Executor::tui_select(pair_cmd);

        if (!pair_code.empty()) {
            Logger::step(_f("termux.adb_pairing", addr));
            auto pair_result = Executor::shell("adb pair " + addr + " " + pair_code + " 2>&1");
            Logger::info(pair_result.stdout_data);
            if (!pair_result.ok()) {
                Logger::warn(_f("termux.adb_pair_may_fail", pair_result.stderr_data));
            }
        }

        // 阶段 2: 连接 (地址可能不同)
        std::string connect_cmd = cfg_.tui_bin + " --title \"" + _("termux.adb_connect2_title") + "\" "
                                  "--inputbox \"" + _f("termux.adb_connect2_input", addr) + "\" 0 50 \"" + addr + "\"";

        std::string connect_addr = Executor::tui_select(connect_cmd);
        if (connect_addr.empty()) {
            Logger::info(_("termux.adb_connect_cancelled"));
            return false;
        }

        Logger::step(_f("termux.adb_connecting", connect_addr));
        auto connect_result = Executor::shell("adb connect " + connect_addr + " 2>&1");
        Logger::info(connect_result.stdout_data);

        if (connect_result.ok() || connect_result.stdout_data.find("connected") != std::string::npos) {
            Logger::ok(_("termux.adb_connect_ok"));
            return true;
        }

        Logger::warn(_("termux.adb_connect_check_device"));
        return false;
    }

    bool TermuxManager::select_adb_port() {
        Logger::step(_("termux.adb_port_configuring"));

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.adb_port_title") + "\" "
                          "--inputbox \"" + _("termux.adb_port_input") + "\" 0 50 \"5037\"";

        std::string port_str = Executor::tui_select(cmd);
        if (port_str.empty()) return false;

        try {
            int port = std::stoi(port_str);
            if (port < 1024 || port > 65535) {
                Logger::error(_f("termux.adb_port_invalid", port_str));
                return false;
            }

            // 设置 ADB_SERVER_PORT 环境变量并重启
            Logger::step(_f("termux.adb_port_switching", port_str));
            Executor::shell("adb kill-server 2>/dev/null");

            // 使用 -P 标志重启
            Executor::shell("adb -P " + port_str + " start-server 2>/dev/null");
            Logger::ok(_f("termux.adb_port_changed", port_str));
            Logger::warn(_f("termux.adb_port_param_note", port_str));

            return true;
        } catch (...) {
            Logger::error(_("termux.adb_port_invalid_format"));
            return false;
        }
    }

    bool TermuxManager::verify_signal9_fix() {
        Logger::step(_("termux.signal9_verify_starting"));

        // 统计设备数量并选择合适的 target
        int device_count = count_adb_devices();
        if (device_count <= 0) {
            Logger::warn(_("termux.signal9_verify_no_device"));
            if (!connect_adb_and_fix()) {
                Logger::error(_("termux.signal9_verify_connect_failed"));
                return false;
            }
            device_count = 1;
        }

        std::string adb_target;
        if (device_count == 1) {
            adb_target = "adb shell";
        } else {
            // 多设备时列出选择
            auto devices = Executor::shell("adb devices 2>/dev/null | grep -v 'List' | grep 'device$'");
            std::istringstream iss(devices.stdout_data);
            std::string line;
            std::vector<std::string> device_ids;
            while (std::getline(iss, line)) {
                size_t tab = line.find('\t');
                if (tab != std::string::npos) {
                    device_ids.push_back(line.substr(0, tab));
                }
            }

            if (device_ids.size() == 1) {
                adb_target = "adb -s " + device_ids[0] + " shell";
            } else if (device_ids.size() > 1) {
                // 简单选择第一个
                adb_target = "adb -s " + device_ids[0] + " shell";
                Logger::info(_f("termux.signal9_multi_device", device_ids[0]));
            } else {
                adb_target = "adb shell";
            }
        }

        // 使用 dumpsys 验证
        auto result = Executor::shell(
            adb_target + " dumpsys activity settings 2>/dev/null | grep max_phantom_processes");
        std::string output = result.stdout_data;
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();

        if (!output.empty()) {
            Logger::ok(_f("termux.signal9_verify_ok", output));
            return true;
        }

        Logger::warn(_("termux.signal9_verify_no_output"));
        return false;
    }

    int TermuxManager::count_adb_devices() {
        auto result = Executor::shell("adb devices 2>/dev/null | grep -v 'List of devices' | grep 'device$' | wc -l");
        std::string out = result.stdout_data;
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();

        try {
            return std::stoi(out);
        } catch (...) {
            return 0;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // GUI 增强
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::auto_start_vnc_viewer() {
        if (!cfg_.is_termux) return;

        Logger::step(_("termux.gui_starting_vnc_viewer"));
        Executor::shell(
            "am start -n com.realvnc.viewer.android/com.realvnc.viewer.android.app.ConnectionChooserActivity 2>/dev/null");
        Logger::info(_("termux.gui_vnc_viewer_hint"));
    }

    void TermuxManager::auto_start_file_manager_in_vnc() {
        Logger::step(_("termux.gui_starting_file_manager"));
        for (const auto &fm: {"thunar", "pcmanfm-qt", "nautilus", "dolphin"}) {
            if (Executor::has(fm)) {
                Executor::shell(std::string(fm) + " & 2>/dev/null");
                Logger::info(_f("termux.gui_file_manager_started", std::string(fm)));
                return;
            }
        }
        Logger::info(_("termux.gui_no_file_manager"));
    }

    std::string TermuxManager::detect_and_show_lan_ip() {
        Logger::step(_("termux.gui_detecting_lan_ip"));

        auto result = Executor::shell("ip -4 -br -c a 2>/dev/null | grep -v '127.0.0.1' | head -3");
        if (result.ok() && !result.stdout_data.empty()) {
            Logger::info(_f("termux.gui_lan_ip_result", result.stdout_data));
            return result.stdout_data;
        }

        // 备用方法: ifconfig
        result = Executor::shell("ifconfig 2>/dev/null | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}'");
        if (result.ok() && !result.stdout_data.empty()) {
            Logger::info(_f("termux.gui_lan_ip_result", result.stdout_data));
            return result.stdout_data;
        }

        Logger::warn(_("termux.gui_lan_ip_not_found"));
        return "";
    }

    std::string TermuxManager::configure_terminal_emulator_for_de(const std::string &desktop_env) {
        if (desktop_env == "xfce") return "aterm";
        if (desktop_env == "lxqt") return "qterminal";
        if (desktop_env == "mate") return "mate-terminal";
        if (desktop_env == "gnome") return "gnome-terminal";
        if (desktop_env == "kde" || desktop_env == "plasma") return "konsole";
        // 默认使用 xfce4-terminal (最常用)
        return "xfce4-terminal";
    }

    void TermuxManager::edit_vnc_config_manually() {
        std::string startvnc = "/data/data/com.termux/files/usr/bin/startvnc";
        if (!fs::exists(startvnc)) {
            Logger::warn(_("termux.gui_startvnc_not_found"));
            return;
        }

        Logger::step(_("termux.gui_editing_startvnc"));
        Executor::shell("nano " + startvnc);
        Logger::ok(_("termux.gui_startvnc_edited"));

        // 编辑后运行 fix-shebang
        run_termux_fix_shebang(startvnc);
    }

    void TermuxManager::run_termux_fix_shebang(const std::string &file_path) {
        if (!Executor::has("termux-fix-shebang")) return;
        CommandBuilder("termux-fix-shebang").add_arg(file_path).add_raw("2>/dev/null").execute();
    }

    void TermuxManager::linux_gui_fallback() {
        if (cfg_.is_termux) return;

        Logger::info(_("termux.gui_linux_fallback"));
        Logger::step(_("termux.gui_downloading_external_tool"));
        Executor::shell(
            "bash -c \"$(curl -L https://gitee.com/mo2/linux/raw/2/2)\" --install-gui "
            "|| bash -c \"$(curl -L https://raw.githubusercontent.com/2cd/linux/2/2)\" --install-gui");
    }

    // ═══════════════════════════════════════════════════════════════
    // 备份增强
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::unmount_before_backup() {
        Logger::step(_("termux.unmount_before_backup"));

        // 调用共享 umount 脚本
        std::string umount_script = cfg_.work_dir.string() + "/share/removal/umount";
        if (fs::exists(umount_script)) {
            Executor::shell("source " + umount_script + " 2>/dev/null || bash " + umount_script);
        } else {
            // 手动卸载常见挂载点
            std::vector<std::string> mount_points = {
                "/data/data/com.termux/files/usr/tmp/.tmoe/containers",
                "/data/data/com.termux/files/home/.tmoe/linux"
            };
            for (auto &mp: mount_points) {
                Executor::shell("umount -l " + mp + " 2>/dev/null");
            }
        }
        Logger::info(_("termux.unmount_done"));
    }

    void TermuxManager::timeshift_backup_option() {
        Logger::step(_("termux.timeshift_backup_integration"));

        auto family = infer_family_from_config(cfg_.linux_distro);

        std::string cmd = cfg_.tui_bin + " --title \"" + _("termux.timeshift_title") + "\" "
                          "--menu \"" + _("termux.timeshift_prompt") + "\" 0 50 0 "
                          "\"1\" \"" + _("termux.timeshift_install") + "\" "
                          "\"2\" \"" + _("termux.timeshift_create") + "\" "
                          "\"3\" \"" + _("termux.timeshift_restore") + "\" "
                          "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") {
            Logger::step(_("termux.timeshift_installing_now"));
            PackageManager::install("timeshift", family);
            if (Executor::has("timeshift")) {
                Logger::ok(_("termux.timeshift_install_ok"));
            } else {
                Logger::error(_("termux.timeshift_install_failed"));
            }
        } else if (choice == "2") {
            if (!Executor::has("timeshift")) {
                Logger::step(_("termux.timeshift_not_installed_yet"));
                PackageManager::install("timeshift", family);
            }
            Logger::step(_("termux.timeshift_creating_snapshot"));
            Executor::shell("sudo timeshift --create --comments \"tmoe-linux auto backup\"");
            Logger::ok(_("termux.timeshift_snapshot_ok"));
        } else if (choice == "3") {
            if (!Executor::has("timeshift")) {
                Logger::error(_("termux.timeshift_not_installed"));
                return;
            }
            Logger::warn(_("termux.timeshift_restore_warning"));
            Executor::shell("sudo timeshift --restore");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // OpenSSL 旧版修复
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::check_openssl_legacy() {
        if (!cfg_.is_termux) return;

        Logger::step(_("termux.openssl_checking_legacy"));

        // 检查 openssl 版本
        auto result = Executor::shell("openssl version 2>/dev/null");
        if (!result.ok()) return;

        std::string ver = result.stdout_data;
        if (ver.find("1.1") != std::string::npos) {
            Logger::info(_("termux.openssl_legacy_already_installed"));
            return;
        }

        // 检测是否需要旧版 openssl-1.1 (某些旧包依赖)
        bool need_legacy = Executor::shell(
            "apt list --installed 2>/dev/null | grep -i openssl | grep '1.1'").ok();

        if (need_legacy) {
            Logger::warn(_("termux.openssl_legacy_needed"));
            Executor::shell("apt install -y openssl-1.1 2>/dev/null || apt install -y openssl 2>/dev/null");
            Logger::ok(_("termux.openssl_legacy_ok"));
        } else {
            Logger::info(_("termux.openssl_legacy_not_needed"));
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 二进制到包映射
    // ═══════════════════════════════════════════════════════════════

    std::string TermuxManager::map_binary_to_termux_pkg(const std::string &binary) {
        // 将命令行工具名称映射到 Termux 包名
        static const std::unordered_map<std::string, std::string> pkg_map = {
            {"xz", "xz-utils"},
            {"aria2c", "aria2"},
            {"pkill", "procps"},
            {"chroot", "coreutils"},
            {"unshare", "util-linux"},
            {"pgrep", "procps"},
            {"pidof", "procps"},
            {"bat", "bat"},
            {"batcat", "bat"},
            {"lsof", "lsof"},
            {"micro", "micro"},
            {"nano", "nano"},
            {"vim", "vim"},
            {"neovim", "neovim"},
            {"tar", "tar"},
            {"gzip", "gzip"},
            {"bzip2", "bzip2"},
            {"zstd", "zstd"},
            {"wget", "wget"},
            {"curl", "curl"},
            {"git", "git"},
            {"proot", "proot"},
        };

        auto it = pkg_map.find(binary);
        if (it != pkg_map.end()) return it->second;
        return binary; // 默认返回原始名称
    }

    // ═══════════════════════════════════════════════════════════════
    // 备用桌面会话
    // ═══════════════════════════════════════════════════════════════

    std::string TermuxManager::get_fallback_xsession(const std::string &desktop_env) {
        if (desktop_env == "xfce") return "xfce4-session";
        if (desktop_env == "lxqt") return "startlxqt";
        if (desktop_env == "mate") return "mate-session";
        if (desktop_env == "gnome") return "gnome-session";
        if (desktop_env == "kde") return "startplasma-x11";
        if (desktop_env == "cinnamon") return "cinnamon-session";
        if (desktop_env == "budgie") return "budgie-desktop";
        return "";
    }
} // namespace tmoe::domain
