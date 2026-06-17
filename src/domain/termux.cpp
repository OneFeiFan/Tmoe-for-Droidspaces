#include "termux.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <regex>
#include <sstream>

namespace tmoe::domain {
    // ═══════════════════════════════════════════════════════════════
    // 环境初始化 (已完成)
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::check_and_init_environment() {
        if (!cfg_.is_termux) return true;

        Logger::step("检查 Termux 沙盒基础环境与依赖...");
        std::string missing_pkgs;

        if (!Executor::has("curl") && !Executor::has("wget")) missing_pkgs += " curl wget";
        if (!Executor::has("git")) missing_pkgs += " git";
        if (!Executor::has("proot")) missing_pkgs += " proot";
        if (!Executor::has("tar") || !Executor::has("xz")) missing_pkgs += " tar xz-utils";

        if (!missing_pkgs.empty()) {
            Logger::warn("Termux 缺失核心运行依赖，正在自动补全:" + missing_pkgs);
            Executor::shell(cfg_.update_command);
            if (!Executor::shell(cfg_.install_command + missing_pkgs).ok()) {
                Logger::error("Termux 依赖自动安装失败！请检查网络连接。");
                return false;
            }
            Logger::ok("Termux 依赖安装完成！");
        }

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

        Logger::step("检查 Termux 共享存储状态...");

        // 检查 storage/shared 软链接是否已存在
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        if (fs::exists(home + "/storage/shared")) {
            Logger::info("共享存储已配置，跳过。");
            return true;
        }

        Logger::warn("未检测到共享存储链接，正在调用 termux-setup-storage...");
        if (!Executor::has("termux-setup-storage")) {
            Logger::step("安装 termux-tools...");
            Executor::shell("apt update && apt install -y termux-tools");
        }

        auto result = Executor::shell("termux-setup-storage");
        if (result.ok()) {
            Logger::ok("Termux 共享存储设置完成！");
            return true;
        }

        Logger::error("termux-setup-storage 执行失败，请手动授权存储权限。");
        return false;
    }

    // ═══════════════════════════════════════════════════════════════
    // X11 / GUI 支持
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::install_x11_support() {
        if (!cfg_.is_termux) {
            Logger::info("非 Termux 环境，跳过 X11 支持安装。");
            return true;
        }

        Logger::step("安装 Termux X11 支持 (x11-repo + TigerVNC)");

        std::string desktop = select_desktop_environment();
        if (desktop.empty()) {
            Logger::info("用户取消了桌面环境选择。");
            return false;
        }

        if (desktop == "xfce") return install_termux_xfce();
        if (desktop == "lxqt") return install_termux_lxqt();

        return false;
    }

    std::string TermuxManager::select_desktop_environment() {
        std::string cmd = cfg_.tui_bin + " --title \"Termux GUI\" "
                          "--menu \"选择要安装的桌面环境\\nChoose desktop environment\" 0 50 0 "
                          "\"1\" \"XFCE4 (轻量推荐)\" "
                          "\"2\" \"LXQt (极简)\" "
                          "\"0\" \"返回 Back\"";

        auto result = Executor::tui_select(cmd);
        if (result == "1") return "xfce";
        if (result == "2") return "lxqt";
        return "";
    }

    bool TermuxManager::install_termux_xfce() {
        Logger::step("安装 Termux 原生 XFCE4 桌面环境...");

        // 1. 安装 x11-repo
        Executor::shell("apt update");
        Executor::shell("apt install -y x11-repo");
        Executor::shell("apt update");
        Executor::shell("apt dist-upgrade -y");

        // 2. 安装 XFCE4 依赖
        std::string deps = "xfce aterm xfce4-terminal tigervnc";
        Executor::shell("apt install -y " + deps);

        // 3. 创建 startvnc 启动脚本
        std::string resolution = select_vnc_resolution();
        create_startvnc_script(resolution, "xfce");

        Logger::ok("XFCE4 桌面环境安装完成！使用 startvnc 启动。");
        Logger::info("VNC 客户端连接: 127.0.0.1:5902");
        return true;
    }

    bool TermuxManager::install_termux_lxqt() {
        Logger::step("安装 Termux 原生 LXQt 桌面环境...");

        Executor::shell("apt update");
        Executor::shell("apt install -y x11-repo");
        Executor::shell("apt update");
        Executor::shell("apt dist-upgrade -y");

        std::string deps = "lxqt qterminal tigervnc";
        Executor::shell("apt install -y " + deps);

        std::string resolution = select_vnc_resolution();
        create_startvnc_script(resolution, "lxqt");

        Logger::ok("LXQt 桌面环境安装完成！使用 startvnc 启动。");
        Logger::info("VNC 客户端连接: 127.0.0.1:5902");
        return true;
    }

    std::string TermuxManager::select_vnc_resolution() {
        std::string cmd = cfg_.tui_bin + " --title \"VNC 分辨率\" "
                          "--menu \"选择 VNC 显示分辨率\\nChoose VNC resolution\" 0 50 0 "
                          "\"1\" \"1280x720 (HD)\" "
                          "\"2\" \"1920x1080 (Full HD)\" "
                          "\"3\" \"720x1440 (手机竖屏)\" "
                          "\"4\" \"2560x1440 (2K)\" "
                          "\"5\" \"自定义 Custom\"";

        auto result = Executor::tui_select(cmd);

        if (result == "1") return "1280x720";
        if (result == "2") return "1920x1080";
        if (result == "3") return "720x1440";
        if (result == "4") return "2560x1440";
        if (result == "5") {
            std::string input_cmd = cfg_.tui_bin + " --title \"自定义分辨率\" "
                                    "--inputbox \"输入分辨率 (例如 1440x2560)\" 0 50";
            auto custom = Executor::tui_select(input_cmd);
            if (!custom.empty()) return custom;
        }
        return "1280x720"; // 默认
    }

    void TermuxManager::create_startvnc_script(const std::string &resolution, const std::string &desktop_env) {
        std::string bin_dir = "/data/data/com.termux/files/usr/bin";
        std::string script_path = bin_dir + "/startvnc";

        std::ofstream ofs(script_path);
        if (!ofs.is_open()) {
            Logger::error("无法创建 startvnc 脚本: " + script_path);
            return;
        }

        ofs << "#!/data/data/com.termux/files/usr/bin/bash\n";
        ofs << "# Auto-generated by tmoe-cpp\n";
        ofs << "export DISPLAY=:2\n";
        ofs << "VNC_RESOLUTION=" << resolution << "\n\n";
        ofs << "pkill Xvnc 2>/dev/null\n";
        ofs << "sleep 1\n\n";
        ofs << "# Start PulseAudio if available\n";
        ofs << "pulseaudio --start 2>/dev/null\n\n";
        ofs << "Xvnc -geometry ${VNC_RESOLUTION} -depth 24 \\\n";
        ofs << "     -desktop termux-" << desktop_env << " \\\n";
        ofs << "     --SecurityTypes=None ${DISPLAY} &\n\n";
        ofs << "sleep 2\n\n";

        if (desktop_env == "xfce") {
            ofs << "dbus-launch --exit-with-session startxfce4 &\n";
        } else if (desktop_env == "lxqt") {
            ofs << "dbus-launch --exit-with-session startlxqt &\n";
        }

        ofs << "\necho \"VNC server running at 127.0.0.1:5902\"\n";
        ofs << "echo \"使用 VNC 客户端连接即可。\"\n";
        ofs << "wait\n";

        ofs.close();
        Executor::shell("chmod +x " + script_path);
        Logger::ok("startvnc 脚本已创建: " + script_path);
    }

    bool TermuxManager::configure_termux_vnc() {
        if (!cfg_.is_termux) return true;

        std::string startvnc = "/data/data/com.termux/files/usr/bin/startvnc";
        if (!fs::exists(startvnc)) {
            Logger::warn("未找到 startvnc 脚本，请先安装 GUI。");
            return false;
        }

        std::string new_resolution = select_vnc_resolution();
        if (new_resolution.empty()) return false;

        // 用 sed 替换 VNC_RESOLUTION 行
        std::string sed_cmd = "sed -i -E 's@^(VNC_RESOLUTION=).*@\\1" + new_resolution + "@' " + startvnc;
        Executor::shell(sed_cmd);

        Logger::ok("VNC 分辨率已更新为: " + new_resolution);
        return true;
    }

    bool TermuxManager::remove_termux_gui() {
        if (!cfg_.is_termux) return true;

        std::string confirm = cfg_.tui_bin + " --title \"移除 GUI\" "
                              "--yesno \"确定要卸载 xfce4,lxqt,tigervnc,x11-repo 吗？\\nRemove GUI packages?\" 0 50";

        if (!Executor::shell(confirm).ok()) {
            Logger::info("用户取消了 GUI 移除。");
            return false;
        }

        Logger::step("正在移除 Termux GUI 包...");
        Executor::shell("apt purge -y ^xfce tigervnc aterm xfce4-terminal 2>/dev/null; true");
        Executor::shell("apt purge -y lxqt qterminal 2>/dev/null; true");
        Executor::shell("apt purge -y x11-repo 2>/dev/null; true");
        Executor::shell("apt autoremove --purge -y");

        // 清理 startvnc 脚本
        std::string startvnc = "/data/data/com.termux/files/usr/bin/startvnc";
        if (fs::exists(startvnc)) fs::remove(startvnc);

        Logger::ok("GUI 包已移除。");
        return true;
    }

    int TermuxManager::run_termux_gui_menu() {
        while (true) {
            std::string cmd = cfg_.tui_bin + " --title \"Termux GUI 管理\" "
                              "--menu \"Termux 原生 GUI 管理\\n\" 0 50 0 "
                              "\"1\" \"安装 XFCE4 桌面\" "
                              "\"2\" \"安装 LXQt 桌面\" "
                              "\"3\" \"修改 VNC 分辨率\" "
                              "\"4\" \"移除 GUI\" "
                              "\"0\" \"返回 Back\"";

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
        std::string cmd = cfg_.tui_bin + " --title \"备份文件名\" "
                          "--inputbox \"输入自定义备份文件名 (可留空使用默认)\\nEnter backup filename\" 0 50 \"termux-backup\"";

        auto name = Executor::tui_select(cmd);
        if (name.empty()) name = "termux-backup";
        return name;
    }

    std::string TermuxManager::select_backup_directories() {
        std::string cmd = cfg_.tui_bin + " --title \"选择备份目录\" "
                          "--checklist \"勾选要备份的目录 (空格选择)\\nSelect directories to backup\" 0 50 0 "
                          "\"home\" \"$HOME 家目录\" ON "
                          "\"usr\" \"$PREFIX/usr 系统目录\" OFF "
                          "3>&1 1>&2 2>&3";

        return Executor::tui_select(cmd);
    }

    bool TermuxManager::backup_termux() {
        if (!cfg_.is_termux) {
            Logger::info("非 Termux 环境，跳过备份。");
            return true;
        }

        Logger::step("备份 Termux");

        // 1. 选择备份目录
        std::string selected = select_backup_directories();
        if (selected.empty()) {
            Logger::info("用户取消了备份目录选择。");
            return false;
        }

        // 2. 准备
        std::string backup_dir, filename, timestamp;
        backup_termux_prepare(backup_dir, filename, timestamp);

        filename = get_backup_filename();
        if (filename.empty()) return false;

        // 3. 选择压缩类型
        std::string compress_cmd = cfg_.tui_bin + " --title \"压缩格式\" "
                                   "--yesno \"使用 zstd 压缩? (No = xz)\\nUse zstd? (No = xz)\" 0 50 "
                                   "--yes-button \"zstd (快)\" --no-button \"xz (小)\"";

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
            Logger::step("正在使用 zstd 压缩备份...");
            Executor::shell("tar --use-compress-program zstd -Ppvcf " + tar_file +
                            " --exclude=" + tmoe_share + "/containers" + tar_targets);
        } else {
            tar_file = backup_dir + "/" + filename + "-" + timestamp + ".tar.xz";
            Logger::step("正在使用 xz 压缩备份 (较慢)...");
            Executor::shell("tar -PJpvcf " + tar_file +
                            " --exclude=" + tmoe_share + "/containers" + tar_targets);
        }

        if (fs::exists(tar_file)) {
            auto fsize = fs::file_size(tar_file);
            Logger::ok("备份完成: " + tar_file + " (" +
                       std::to_string(fsize / 1024 / 1024) + " MB)");
            return true;
        }

        Logger::error("备份失败！");
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
            Logger::error("备份目录不存在: " + backup_dir);
            return "";
        }

        // 列出所有备份文件
        Logger::step("扫描备份文件...");
        std::string ls_result = Executor::shell("ls -1th " + backup_dir + "/*termux*bak.tar* 2>/dev/null").stdout_data;

        if (ls_result.empty()) {
            Logger::error("未找到任何备份文件。");
            return "";
        }

        // 构建 whiptail 菜单
        std::stringstream menu_builder;
        menu_builder << cfg_.tui_bin << " --title \"选择备份文件\" "
                << "--menu \"Choose backup file\\n选择备份文件\" 0 50 20 ";

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
        menu_builder << "\"0\" \"返回 Back\"";
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
            Logger::info("非 Termux 环境，跳过还原。");
            return true;
        }

        Logger::step("还原 Termux");

        std::string restore_file;
        if (!archive_path.empty()) {
            restore_file = archive_path;
        } else {
            // 检查是否有最新备份
            std::string latest = detect_latest_backup();
            if (!latest.empty()) {
                std::string confirm = cfg_.tui_bin + " --title \"还原 Termux\" "
                                      "--yesno \"检测到最新备份:\\n" + latest +
                                      "\\n\\n是否使用此备份还原？\\nUse latest backup?\" 0 50";
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
            Logger::info("用户取消了还原。");
            return false;
        }

        // 警告
        std::string warn = cfg_.tui_bin + " --title \"⚠️ 警告\" "
                           "--yesno \"还原将覆盖当前 Termux 数据！\\n继续？\\n\\nRESTORE WILL OVERWRITE CURRENT DATA!\" 0 50";
        if (!Executor::shell(warn).ok()) {
            Logger::info("用户取消了还原。");
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
        Logger::step("解压 zst 备份: " + file);

        if (Executor::has("pv")) {
            Executor::shell("pv " + file + " | tar --use-compress-program zstd -Ppx");
        } else {
            Executor::shell("tar --use-compress-program zstd -Ppxvf " + file);
        }

        Logger::ok("Termux 还原完成！请重启 Termux 使更改生效。");
        return true;
    }

    bool TermuxManager::uncompress_xz(const std::string &file) {
        Logger::step("解压 xz 备份: " + file);

        if (Executor::has("pv")) {
            Executor::shell("pv " + file + " | tar -PpJx");
        } else {
            Executor::shell("tar -PpJxvf " + file);
        }

        Logger::ok("Termux 还原完成！请重启 Termux 使更改生效。");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 终端美化
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::beautify_terminal() {
        Logger::step("终端美化");

        std::string cmd = cfg_.tui_bin + " --title \"终端美化\" "
                          "--menu \"选择美化方式\\nChoose beautification\" 0 50 0 "
                          "\"1\" \"配置 tmoe-zsh (推荐)\" "
                          "\"2\" \"安装 oh-my-zsh\" "
                          "\"3\" \"安装 powerlevel10k 主题\" "
                          "\"4\" \"安装 colorls\" "
                          "\"0\" \"返回 Back\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") return configure_tmoe_zsh();
        if (choice == "2") {
            Logger::step("安装 oh-my-zsh...");
            Executor::shell("apt install -y zsh git curl");
            Executor::shell(
                "sh -c \"$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\" \"\" --unattended");
            Logger::ok("oh-my-zsh 安装完成！");
            return true;
        }
        if (choice == "3") {
            Logger::step("安装 powerlevel10k...");
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
            std::string p10k_dir = home + "/.oh-my-zsh/custom/themes/powerlevel10k";
            if (!fs::exists(p10k_dir)) {
                Executor::shell("git clone --depth=1 https://github.com/romkatv/powerlevel10k.git " + p10k_dir);
            }
            // 更新 .zshrc 中的 ZSH_THEME
            std::string zshrc = home + "/.zshrc";
            if (fs::exists(zshrc)) {
                Executor::shell("sed -i 's/^ZSH_THEME=.*/ZSH_THEME=\"powerlevel10k\\/powerlevel10k\"/' " + zshrc);
            }
            Logger::ok("powerlevel10k 安装完成！重启终端生效。");
            return true;
        }
        if (choice == "4") {
            Logger::step("安装 colorls...");
            Executor::shell("apt install -y ruby");
            Executor::shell("gem install colorls");
            Logger::ok("colorls 安装完成！");
            return true;
        }
        return false;
    }

    bool TermuxManager::configure_tmoe_zsh() {
        Logger::step("配置 tmoe-zsh 工具箱...");

        // 检测是否已配置
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
        std::string git_config = home + "/.config/tmoe-zsh/git/.git/config";

        if (fs::exists(git_config)) {
            Logger::info("tmoe-zsh 已配置。如需重新配置请删除 ~/.config/tmoe-zsh 后重试。");
            return true;
        }

        // 安装依赖
        std::string confirm = cfg_.tui_bin + " --title \"tmoe-zsh\" "
                              "--yesno \"tmoe-zsh 提供 zinit + 主题 + 插件 一站式配置。\\n将下载约 10MB 文件，继续？\" 0 50";
        if (!Executor::shell(confirm).ok()) {
            Logger::info("用户取消了 tmoe-zsh 配置。");
            return false;
        }

        // 下载并运行安装脚本
        std::string install_bin = "/data/data/com.termux/files/usr/local/bin/zsh-i";
        std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/zsh.sh";

        Logger::step("下载 tmoe-zsh 安装脚本...");
        Executor::shell("mkdir -p /data/data/com.termux/files/usr/local/bin");
        Executor::shell("curl -Lo " + install_bin + " " + url);
        Executor::shell("chmod 777 " + install_bin);

        Logger::step("运行 tmoe-zsh 自动配置 (tmoe_container_auto_configure)...");
        Executor::shell("bash " + install_bin + " --tmoe_container_auto_configure");

        Logger::ok("tmoe-zsh 配置完成！支持主题切换: zsh-i --theme");
        return true;
    }

    bool TermuxManager::change_shell_to_zsh() {
        if (!Executor::has("zsh")) {
            Logger::warn("zsh 未安装，正在自动安装...");
            Executor::shell("apt install -y zsh");
        }

        // 检查当前 shell
        auto current = Executor::shell("echo $SHELL");
        if (current.stdout_data.find("zsh") != std::string::npos) {
            Logger::info("当前已使用 zsh。");
            return true;
        }

        if (cfg_.is_termux) {
            // Termux 使用 chsh
            Executor::shell("chsh -s zsh");
        } else {
            // 容器内修改 /etc/passwd
            Executor::shell("sed -E -i 's@(root:x:0:0:root:/root:/bin/)(ash|bash)@\\1zsh@' /etc/passwd");
        }

        Logger::ok("默认 shell 已切换为 zsh。重启终端生效。");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 软件包镜像源
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::switch_pkg_mirror(std::string_view mirror) {
        if (!cfg_.is_termux) {
            Logger::info("非 Termux 环境，跳过镜像源切换。");
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
        std::string cmd = cfg_.tui_bin + " --title \"Termux 软件源管理\" "
                          "--menu \"Termux 软件包镜像源管理\\nPackage mirror management\" 0 50 0 "
                          "\"1\" \"北外镜像站 (BFSU)\" "
                          "\"2\" \"腾讯云镜像站 (Tencent)\" "
                          "\"3\" \"清华镜像站 (Tsinghua)\" "
                          "\"4\" \"中科大镜像站 (USTC)\" "
                          "\"5\" \"启用/禁用仓库 (game/root/science...)\" "
                          "\"6\" \"镜像站下载测速\" "
                          "\"7\" \"手动编辑 sources.list\" "
                          "\"8\" \"清理无效行\" "
                          "\"9\" \"恢复默认官方源\" "
                          "\"0\" \"返回 Back\"";

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
        else return false;

        return true;
    }

    void TermuxManager::modify_termux_sources_list(const std::string &mirror_url) {
        Logger::step("切换 Termux 镜像源: " + mirror_url);

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
        Logger::step("更新软件包数据库...");
        Executor::shell("apt update");
        Logger::ok("Termux 镜像源切换完成！");
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
        Executor::shell("sed -i 's/^\\(deb .*\\)/# \\1/' " + file_path);

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

        Logger::step("Termux 仓库管理");

        std::string cmd = cfg_.tui_bin + " --title \"仓库管理\" "
                          "--menu \"启用/禁用 Termux 额外仓库\\nEnable/disable repos\" 0 50 0 "
                          "\"1\" \"🐧 root-repo (root 权限工具)\" "
                          "\"2\" \"🖥️ x11-repo (GUI 支持)\" "
                          "\"3\" \"🎮 game-repo (游戏)\" "
                          "\"4\" \"🔬 science-repo (科学计算)\" "
                          "\"5\" \"⚠️ unstable-repo (不稳定版)\" "
                          "\"0\" \"返回 Back\"";

        auto choice = Executor::tui_select(cmd);
        if (choice == "0" || choice.empty()) return;

        std::string repos[] = {"", "root", "x11", "game", "science", "unstable"};
        int idx = std::stoi(choice);
        if (idx < 1 || idx > 5) return;

        std::string repo = repos[idx];
        std::string action = cfg_.tui_bin + " --title \"" + repo + "-repo\" "
                             "--yesno \"启用还是禁用 " + repo + "-repo?\" 0 50 "
                             "--yes-button \"启用 enable\" --no-button \"禁用 disable\"";

        if (Executor::shell(action).ok()) {
            Logger::step("启用 " + repo + "-repo...");
            Executor::shell("apt update");
            Executor::shell("apt install -y " + repo + "-repo");
            Executor::shell("apt update");
            Logger::ok(repo + "-repo 已启用。");
        } else {
            Logger::step("禁用 " + repo + "-repo...");
            Executor::shell("apt purge -y " + repo + "-repo 2>/dev/null; true");
            Executor::shell("apt update");
            Logger::ok(repo + "-repo 已禁用。");
        }
    }

    void TermuxManager::run_mirror_speed_test() {
        if (!cfg_.is_termux) return;

        Logger::step("镜像站下载速度测试");

        // 确保 aria2c 可用
        if (!Executor::has("aria2c")) {
            Logger::step("安装 aria2...");
            Executor::shell("apt install -y aria2");
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

        Logger::info("镜像站           速度");
        Logger::info("────────────────────────");

        std::string temp_dir = "/data/data/com.termux/files/home/.tmoe_speed_test";
        fs::create_directories(temp_dir);

        for (auto &m: mirrors) {
            // 获取最新 clang .deb 文件名
            auto deb_name_result = Executor::shell(
                "curl -sL " + m.url + " | grep -oP 'clang_[^\"]*\\.deb' | head -1");

            std::string deb_name = deb_name_result.stdout_data;
            while (!deb_name.empty() && (deb_name.back() == '\n' || deb_name.back() == '\r')) deb_name.pop_back();

            if (deb_name.empty()) {
                Logger::info(std::string(m.name) + "  ───  (获取文件列表失败)");
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
        Logger::ok("测速完成！");
    }

    void TermuxManager::edit_sources_manually() {
        if (!cfg_.is_termux) return;

        std::string editor = "nano";
        if (Executor::has("vim")) editor = "vim";
        if (Executor::has("micro")) editor = "micro";

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string sources = prefix + "/etc/apt/sources.list";

        Logger::step("使用 " + editor + " 编辑 " + sources);
        Executor::shell(editor + " " + sources);
    }

    void TermuxManager::clean_sources_list() {
        if (!cfg_.is_termux) return;

        Logger::step("清理 sources.list 中的无效/重复行...");

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string apt_dir = prefix + "/etc/apt";

        // 处理主 sources.list
        std::string main_file = apt_dir + "/sources.list";
        if (fs::exists(main_file)) {
            Executor::shell("sed -i '/^#/d' " + main_file);
            Executor::shell("sort -u -o " + main_file + " " + main_file);
        }

        // 处理 sources.list.d/*.list
        std::string list_d = apt_dir + "/sources.list.d";
        if (fs::exists(list_d)) {
            for (auto &entry: fs::directory_iterator(list_d)) {
                if (entry.path().extension() == ".list") {
                    std::string p = entry.path().string();
                    Executor::shell("sed -i '/^#/d' " + p);
                    Executor::shell("sort -u -o " + p + " " + p);
                }
            }
        }

        Logger::ok("sources.list 已清理。");
    }

    void TermuxManager::restore_default_sources() {
        if (!cfg_.is_termux) return;

        std::string confirm = cfg_.tui_bin + " --title \"恢复默认源\" "
                              "--yesno \"将 sources.list 恢复为 Termux 官方默认源？\" 0 50";
        if (!Executor::shell(confirm).ok()) return;

        Logger::step("恢复 Termux 默认官方源...");

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

        Logger::ok("已恢复默认官方源！");
        Executor::shell("apt update");
    }

    // ═══════════════════════════════════════════════════════════════
    // Android 12+ Signal 9 修复
    // ═══════════════════════════════════════════════════════════════

    bool TermuxManager::fix_android_12_signal_9() {
        if (!cfg_.is_termux) {
            Logger::info("非 Termux 环境，跳过 Signal 9 修复。");
            return true;
        }

        Logger::step("Android 12+ Signal 9 (Phantom Process) 修复向导");

        // 询问用户是否遇到该问题
        std::string ask = cfg_.tui_bin + " --title \"Signal 9 问题检测\" "
                          "--yesno \"是否遇到 Termux 进程被系统强制杀死 (Signal 9) 的问题？\\n\\n"
                          "(Android 12+ 会限制后台进程数，导致 Termux 意外退出)\" 0 50";
        if (!Executor::shell(ask).ok()) {
            Logger::info("未遇到 Signal 9 问题，跳过修复。");
            return true;
        }

        return connect_adb_and_fix();
    }

    bool TermuxManager::connect_adb_and_fix() {
        // 1. 确保 adb 已安装
        if (!Executor::has("adb")) {
            Logger::step("安装 android-tools (adb)...");
            Executor::shell("apt update && apt install -y android-tools");
        }

        // 2. 三星设备检测
        bool samsung = is_samsung_device();

        if (samsung) {
            Logger::warn("检测到三星设备，需特殊处理 adb 连接。");
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";
            std::string tmpdir = home + "/.tmoe_tmp";
            fs::create_directories(tmpdir);
        }

        // 3. 连接方式选择
        std::string method = cfg_.tui_bin + " --title \"ADB 连接\" "
                             "--menu \"选择连接方式\\nConnect method\" 0 50 0 "
                             "\"1\" \"无线调试 (配对+连接)\" "
                             "\"2\" \"仅连接 (已知设备)\" "
                             "\"3\" \"USB 连接\" "
                             "\"0\" \"跳过 (仅显示手动命令)\"";

        auto choice = Executor::tui_select(method);
        std::string adb_target;

        if (choice == "1") {
            // 配对模式
            std::string pair_cmd = cfg_.tui_bin + " --title \"配对地址\" "
                                   "--inputbox \"输入配对地址和端口 (例如 192.168.1.100:45678)\" 0 50";
            std::string pair_addr = Executor::tui_select(pair_cmd);
            if (!pair_addr.empty()) {
                Logger::step("正在配对: " + pair_addr);
                if (samsung) {
                    Executor::shell("fakeroot adb pair " + pair_addr);
                } else {
                    Executor::shell("adb pair " + pair_addr);
                }
            }

            // 连接
            std::string conn_cmd = cfg_.tui_bin + " --title \"连接地址\" "
                                   "--inputbox \"输入连接地址和端口 (例如 192.168.1.100:5555)\" 0 50";
            adb_target = Executor::tui_select(conn_cmd);
            if (!adb_target.empty()) {
                Logger::step("正在连接: " + adb_target);
                Executor::shell("adb connect " + adb_target);
            }
        } else if (choice == "2") {
            std::string conn_cmd = cfg_.tui_bin + " --title \"连接地址\" "
                                   "--inputbox \"输入 IP:PORT\" 0 50";
            adb_target = Executor::tui_select(conn_cmd);
            if (!adb_target.empty()) {
                Executor::shell("adb connect " + adb_target);
            }
        } else if (choice == "3") {
            // USB — 无需额外操作
            Logger::info("请确保 USB 调试已开启并授权。");
        } else {
            // 仅显示手动命令
            Logger::info("手动修复命令：");
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
        return mfr.find("samsung") != std::string::npos;
    }

    bool TermuxManager::execute_max_phantom_fix(const std::string &adb_target) {
        Logger::step("执行 Signal 9 修复...");

        std::string adb_prefix = "adb";
        if (!adb_target.empty()) {
            adb_prefix += " -s " + adb_target;
        }

        // 检查设备连接
        auto devices = Executor::shell("adb devices -l 2>/dev/null");
        if (devices.stdout_data.find("device") == std::string::npos) {
            Logger::error("未检测到已连接的 ADB 设备！请确保 USB 调试已开启。");
            Logger::info("手动命令：");
            Logger::info("adb shell \"device_config put activity_manager max_phantom_processes 2147483647\"");
            return false;
        }

        // 命令 1: 设置最大 phantom 进程数为 2147483647
        Logger::step("设置 max_phantom_processes = 2147483647...");
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\" 2>/dev/null; true");

        // 命令 2: 禁用 phantom 进程监控
        Logger::step("禁用 phantom 进程监控...");
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/settings put global settings_enable_monitor_phantom_procs false\" 2>/dev/null; true");

        // 命令 3: 持久化 (set_sync_disabled_for_tests)
        Logger::step("持久化设置...");
        Executor::shell(
            adb_prefix +
            " shell \"/system/bin/device_config set_sync_disabled_for_tests persistent\" 2>/dev/null; true");

        Logger::ok("Signal 9 修复已执行！建议重启 Termux 使更改生效。");
        Logger::info("如果仍然遇到问题，请尝试重启手机。");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════
    // 磁盘空间占用
    // ═══════════════════════════════════════════════════════════════

    void TermuxManager::check_disk_usage() {
        if (!cfg_.is_termux) {
            Logger::info("非 Termux 环境。");
            return;
        }

        std::string cmd = cfg_.tui_bin + " --title \"磁盘空间查询\" "
                          "--menu \"Termux 磁盘空间占用查询\\nDisk usage query\" 0 50 0 "
                          "\"1\" \"📁 目录大小排行 (home/usr)\" "
                          "\"2\" \"📄 最大文件 TOP30\" "
                          "\"3\" \"💾 SD 卡占用 (sdcard)\" "
                          "\"4\" \"📊 总磁盘用量 (df -h)\" "
                          "\"0\" \"返回 Back\"";

        auto choice = Executor::tui_select(cmd);

        if (choice == "1") show_termux_dir_usage();
        else if (choice == "2") show_termux_large_files();
        else if (choice == "3") show_sdcard_usage();
        else if (choice == "4") show_overall_disk_usage();
    }

    void TermuxManager::show_termux_dir_usage() {
        Logger::step("Termux 目录大小排行");

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files/usr";
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home";

        Logger::info("── 家目录 TOP15 ──");
        Executor::shell("cd " + home + " && du -hsx ./* ./.* 2>/dev/null | sort -rh | head -15");

        Logger::info("── usr 目录 TOP6 ──");
        Executor::shell("cd " + prefix + " && du -hsx ./* 2>/dev/null | sort -rh | head -6");

        Logger::info("── usr/lib TOP8 ──");
        std::string usr_lib = prefix + "/lib";
        if (fs::exists(usr_lib)) {
            Executor::shell("du -hsx " + usr_lib + "/* 2>/dev/null | sort -rh | head -8");
        }

        Logger::info("── usr/share TOP8 ──");
        std::string usr_share = prefix + "/share";
        if (fs::exists(usr_share)) {
            Executor::shell("du -hsx " + usr_share + "/* 2>/dev/null | sort -rh | head -8");
        }

        Logger::ok("目录大小查询完成！");
    }

    void TermuxManager::show_termux_large_files() {
        Logger::step("Termux 最大文件 TOP30");

        std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/data/data/com.termux/files";
        Executor::shell(
            "cd " + prefix +
            " && find ./ -type f -print0 2>/dev/null | xargs -0 du 2>/dev/null | sort -n | tail -30 | cut -f2 | xargs -I{} du -sh {} 2>/dev/null");

        Logger::ok("文件查询完成！");
    }

    void TermuxManager::show_sdcard_usage() {
        Logger::step("SD 卡空间占用");

        if (!fs::exists("/sdcard")) {
            Logger::warn("未找到 /sdcard 目录。");
            return;
        }

        Logger::info("── 目录大小 TOP15 ──");
        Executor::shell("cd /sdcard && du -hsx ./* ./.* 2>/dev/null | sort -rh | head -15");

        Logger::info("── 最大文件 TOP30 ──");
        Executor::shell(
            "cd /sdcard && find ./ -type f -print0 2>/dev/null | xargs -0 du 2>/dev/null | sort -n | tail -30 | cut -f2 | xargs -I{} du -sh {} 2>/dev/null");

        Logger::ok("SD 卡查询完成！");
    }

    void TermuxManager::show_overall_disk_usage() {
        Logger::step("总磁盘用量");
        Executor::shell("df -h | grep G | grep -v tmpfs");
        Logger::ok("磁盘查询完成！");
    }

    // ═══════════════════════════════════════════════════════════════
    // TUI 兼容补丁 (已完成)
    // ═══════════════════════════════════════════════════════════════

    std::string TermuxManager::check_and_patch_tui_env() {
        if (!cfg_.is_termux) return "whiptail";

        if (!Executor::has("whiptail")) {
            Logger::step("检测到未安装 whiptail，正在自动补充...");
            Executor::shell("apt update && apt install -y whiptail dialog");
        }

        auto dpkg_query = Executor::shell("LANG=C dpkg-query -W libnewt 2>/dev/null");
        if (dpkg_query.stdout_data.find("0.52.21") != std::string::npos) {
            Logger::warn("检测到有缺陷的 libnewt 版本，正在准备兼容性补丁...");

            std::string lib_popt = cfg_.work_dir.string() + "/usr/lib/popt0/0.so";
            std::string wrapper = cfg_.work_dir.string() + "/usr/bin/whiptail-wrapper";
            std::string bin_dir = cfg_.work_dir.string() + "/usr/bin";

            if (!std::filesystem::exists(lib_popt)) {
                Logger::step("正在拉取对应架构的 libpopt 预编译库...");
                std::string arch = cfg_.arch;
                std::string uri = "https://packages.tmoe.me/patch/termux/libp/libpopt0_1.18_" + arch + ".tar.gz";
                std::string tar_file = cfg_.temp_dir.string() + "/libpopt.tar.gz";

                Executor::shell("curl -Lo " + tar_file + " " + uri);
                Executor::shell("tar -zxvf " + tar_file + " -C " + cfg_.work_dir.string());
                Executor::shell("rm -f " + tar_file);
            }

            if (std::filesystem::exists(lib_popt)) {
                std::filesystem::create_directories(bin_dir);
                std::ofstream ofs(wrapper);
                if (ofs.is_open()) {
                    ofs << "#!/usr/bin/env bash\n";
                    ofs << "env LD_PRELOAD=" << lib_popt << " whiptail \"$@\"\n";
                    ofs.close();
                    Executor::shell("chmod +x " + wrapper);
                    Logger::ok("Android TUI 兼容性补丁 (LD_PRELOAD) 应用成功！");
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
        std::string cmd = cfg_.tui_bin + " --title \"COLOR SCHEMES\" "
                          "--menu \"Your colors.properties is empty, please choose color scheme of termux.\\n请选择终端配色\" 0 50 0 "
                          "\"1\" \"neon\" "
                          "\"2\" \"monokai.dark\" "
                          "\"3\" \"material(Cyan)\" "
                          "\"4\" \"bright.light\" "
                          "\"5\" \"materia(Orange)\" "
                          "\"6\" \"miu\" "
                          "\"7\" \"wild.cherry(Purple)\" "
                          "\"0\" \"skip(跳过)\"";

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
            Logger::step("正在下载配色方案: " + color_file);
            std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/colors/" + color_file;
            std::string dest = std::string(std::getenv("HOME")) + "/.termux/colors.properties";
            Executor::shell("curl -L -o " + dest + " " + url);
        }
    }

    void TermuxManager::termux_font_menu() {
        std::string cmd = cfg_.tui_bin + " --title \"FONTS\" "
                          "--menu \"Your font file does not exist, please choose a font.\\n请选择终端字体,若跳过则部分字符可能无法正常显示\" 0 50 0 "
                          "\"1\" \"Inconsolata-go(粗)\" "
                          "\"2\" \"Iosevka(细)\" "
                          "\"3\" \"Iosevka Term Bold Italic(斜)\" "
                          "\"4\" \"Iosevka Term Mono\" "
                          "\"5\" \"Fira code(细)\" "
                          "\"6\" \"Fira code Medium\" "
                          "\"0\" \"skip(跳过)\"";

        std::string choice = Executor::tui_select(cmd);
        std::string font_path;

        if (choice == "1") font_path = "inconsolata-go-font/raw/master/inconsolatago.tar.xz";
        else if (choice == "2") font_path = "inconsolata-go-font/raw/master/iosevka.tar.xz";
        else if (choice == "3") font_path = "iosevka-italic-font/raw/master/font.tar.xz";
        else if (choice == "4") font_path = "iosevka-term-mono/raw/master/font.tar.xz";
        else if (choice == "5") font_path = "fira-code/raw/master/font.tar.xz";
        else if (choice == "6") font_path = "fira-code-medium/raw/master/font.tar.xz";

        if (!font_path.empty()) {
            Logger::step("正在下载并解压字体...");
            std::string termux_dir = std::string(std::getenv("HOME")) + "/.termux";
            std::string url = "https://gitee.com/ak2/" + font_path;
            std::string tar_file = termux_dir + "/font.tar.xz";

            Executor::shell("curl -L -o " + tar_file + " " + url);
            Executor::shell("tar -Jxvf " + tar_file + " -C " + termux_dir);
            Executor::shell("rm -f " + tar_file);
        }
    }

    void TermuxManager::configure_extra_keys() {
        std::string termux_dir = std::string(std::getenv("HOME")) + "/.termux";
        std::string prop_file = termux_dir + "/termux.properties";

        if (!fs::exists(prop_file) || Executor::shell("grep -q '# extra-keys-style = default' " + prop_file).ok()) {
            std::string cmd = cfg_.tui_bin + " --title \"termux.properties\" "
                              "--yesno \"Your extra-keys-style is default, do you want to configure it?\\n是否需要创建termux.properties？这将会修改小键盘布局。\" 10 50";

            if (Executor::shell(cmd).ok()) {
                Logger::step("正在配置 Termux 拓展按键布局...");
                std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/termux.properties";
                std::string tmp_file = termux_dir + "/termux.properties.02";

                if (fs::exists(prop_file)) Executor::shell("cp -f " + prop_file + " " + prop_file + ".bak");
                Executor::shell("curl -L -o " + tmp_file + " " + url);

                Executor::shell(
                    "sed -i -E 's@# (extra-keys-style)@#\\1@g;s@^[^#]@#&@g;1r " + tmp_file + "' " + prop_file);

                if (!fs::exists(prop_file)) {
                    Executor::shell("mv -f " + tmp_file + " " + prop_file);
                } else {
                    Executor::shell("rm -f " + tmp_file);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 主菜单调度
    // ═══════════════════════════════════════════════════════════════

    int TermuxManager::run_termux_menu() {
        while (true) {
            std::string cmd = cfg_.tui_bin + " --title \"📱 Termux 专用功能\" "
                              "--menu \"Termux 功能管理\\n\" 0 50 0 "
                              "\"1\" \"🖥️ Termux 原生 GUI (X11/VNC)\" "
                              "\"2\" \"💾 备份 Termux\" "
                              "\"3\" \"📥 还原 Termux\" "
                              "\"4\" \"🎨 终端美化 (oh-my-zsh)\" "
                              "\"5\" \"📦 软件源管理 (镜像切换)\" "
                              "\"6\" \"📊 磁盘空间查询\" "
                              "\"7\" \"🔧 环境初始化 (配色/字体/按键)\" "
                              "\"8\" \"🩹 Signal 9 修复 (Android 12+)\" "
                              "\"9\" \"📂 共享存储设置\" "
                              "\"0\" \"返回 Back\"";

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
            else if (choice == "0" || choice.empty()) return 0;
        }
    }
} // namespace tmoe::domain
