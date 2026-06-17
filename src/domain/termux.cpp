#include "termux.h"

namespace tmoe::domain {
    bool TermuxManager::install_x11_support() {
        Logger::step("安装 Termux X11 支持");
        // TODO: pkg install x11-repo + termux-x11
        return true;
    }

    bool TermuxManager::backup_termux() {
        Logger::step("备份 Termux");
        // TODO: tar -czf 到外部存储
        return true;
    }

    bool TermuxManager::restore_termux(std::string_view archive_path) {
        Logger::step("恢复 Termux: " + std::string(archive_path));
        // TODO: tar -xzf 到 data/data/com.termux
        return true;
    }

    bool TermuxManager::beautify_terminal() {
        Logger::step("美化终端");
        // TODO: 安装 oh-my-zsh / colorls / 字体
        return true;
    }

    bool TermuxManager::switch_pkg_mirror(std::string_view mirror) {
        Logger::step("切换 Termux 软件源: " + std::string(mirror));
        // TODO: 用选定的镜像重写 sources.list
        return true;
    }

    bool TermuxManager::check_and_init_environment() {
        if (!cfg_.is_termux) return true;

        Logger::step("检查 Termux 沙盒基础环境与依赖...");
        std::string missing_pkgs = "";

        // 1. 下载工具
        if (!Executor::has("curl") && !Executor::has("wget")) {
            missing_pkgs += " curl wget";
        }

        // 2. Git
        if (!Executor::has("git")) {
            missing_pkgs += " git";
        }

        // 3. 核心容器引擎
        if (!Executor::has("proot")) {
            missing_pkgs += " proot";
        }

        // 4. 解压工具
        if (!Executor::has("tar") || !Executor::has("xz")) {
            missing_pkgs += " tar xz-utils";
        }

        if (!missing_pkgs.empty()) {
            Logger::warn("Termux 缺失核心运行依赖，正在自动补全:" + missing_pkgs);

            Executor::shell(cfg_.update_command);

            if (Executor::shell(cfg_.install_command + missing_pkgs).ok()) {
                Logger::ok("Termux 依赖安装完成！");
            } else {
                Logger::error("Termux 依赖自动安装失败！请检查网络连接。");
                return false;
            }
        }

        // 5. 配色方案和字体设置
        std::string home = std::getenv("HOME");
        std::string termux_dir = home + "/.termux";
        fs::create_directories(termux_dir);

        if (!fs::exists(termux_dir + "/colors.properties")) {
            termux_color_scheme_menu();
        }

        if (!fs::exists(termux_dir + "/font.ttf")) {
            termux_font_menu();
        }

        configure_extra_keys();

        Executor::shell("termux-reload-settings");

        return true;
    }

    bool TermuxManager::fix_android_12_signal_9() {
        Logger::step("正在引导修复 Android 12+ Signal 9 (Phantom Process) 崩溃问题...");
        Logger::info("此操作通常需要通过 adb 执行以下命令：");
        Logger::info("adb shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\"");
        // TODO: 通过 Executor::shell 实现原版 fix_signal9 脚本逻辑
        return true;
    }

    std::string TermuxManager::check_and_patch_tui_env() {
        if (!cfg_.is_termux) return "whiptail";

        // 1. 确保 whiptail 和 dialog 已安装
        if (!Executor::has("whiptail")) {
            Logger::step("检测到未安装 whiptail，正在自动补充...");
            Executor::shell("apt update && apt install -y whiptail dialog");
        }

        // 2. 检测有缺陷的 libnewt 0.52.21
        auto dpkg_query = Executor::shell("LANG=C dpkg-query -W libnewt 2>/dev/null");
        if (dpkg_query.stdout_data.find("0.52.21") != std::string::npos) {
            Logger::warn("检测到有缺陷的 libnewt 版本，正在准备兼容性补丁...");

            std::string lib_popt = cfg_.work_dir.string() + "/usr/lib/popt0/0.so";
            std::string wrapper = cfg_.work_dir.string() + "/usr/bin/whiptail-wrapper";
            std::string bin_dir = cfg_.work_dir.string() + "/usr/bin";

            // 如果尚未缓存，则下载预编译的 libpopt
            if (!std::filesystem::exists(lib_popt)) {
                Logger::step("正在拉取对应架构的 libpopt 预编译库...");
                std::string arch = cfg_.arch;
                std::string uri = "https://packages.tmoe.me/patch/termux/libp/libpopt0_1.18_" + arch + ".tar.gz";
                std::string tar_file = cfg_.temp_dir.string() + "/libpopt.tar.gz";

                Executor::shell("curl -Lo " + tar_file + " " + uri);
                Executor::shell("tar -zxvf " + tar_file + " -C " + cfg_.work_dir.string());
                Executor::shell("rm -f " + tar_file);
            }

            // 生成 LD_PRELOAD 包装脚本
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

        // 下载路径与原版 Bash 脚本一致
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

        // 如果文件缺失或保持默认样式，则配置拓展按键
        if (!fs::exists(prop_file) || Executor::shell("grep -q '# extra-keys-style = default' " + prop_file).ok()) {
            std::string cmd = cfg_.tui_bin + " --title \"termux.properties\" "
                              "--yesno \"Your extra-keys-style is default, do you want to configure it?\\n是否需要创建termux.properties？这将会修改小键盘布局。\" 10 50";

            if (Executor::shell(cmd).ok()) {
                Logger::step("正在配置 Termux 拓展按键布局...");
                std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/termux.properties";
                std::string tmp_file = termux_dir + "/termux.properties.02";

                if (fs::exists(prop_file)) Executor::shell("cp -f " + prop_file + " " + prop_file + ".bak");
                Executor::shell("curl -L -o " + tmp_file + " " + url);

                // sed 魔法: 启用拓展按键并追加新配置
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
} // namespace tmoe::domain
