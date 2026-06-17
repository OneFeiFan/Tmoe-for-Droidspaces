#include "environment.h"

namespace tmoe::domain {
    bool Environment::initialize() {
        Logger::step("初始化 tmoes 工作环境");
        cfg_.ensure_dirs();
        // TODO: 设置临时文件、清理残留状态
        return true;
    }

    bool Environment::check_dependencies() {
        if (cfg_.is_termux) {
            // Android 依赖由 TermuxManager 接管处理
            return true;
        }

        Logger::step("检查宿主机系统级依赖...");
        std::string missing_pkgs = "";

        // 1. 检查 sudo (Alpine 除外)
        if (!Executor::has("sudo") && cfg_.linux_distro != "alpine") {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-admin/sudo" : " sudo";
        }

        // 2. 检查 TUI 核心组件: whiptail / dialog
        if (!Executor::has("whiptail") && !Executor::has("dialog")) {
            if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
                missing_pkgs += (cfg_.linux_distro == "gentoo") ? " dev-libs/newt" : " libnewt";
            } else if (cfg_.linux_distro == "openwrt") {
                missing_pkgs += " whiptail dialog";
            } else {
                missing_pkgs += " whiptail";
            }
        }

        // 3. 检查网络工具
        if (!Executor::has("curl") && !Executor::has("wget")) {
            missing_pkgs += " curl wget";
        }

        if (!missing_pkgs.empty()) {
            Logger::warn("缺失核心依赖，正在调用包管理器自动安装: " + missing_pkgs);
            Logger::info("Command: " + cfg_.install_command + missing_pkgs);

            Executor::shell(cfg_.update_command);

            if (Executor::shell(cfg_.install_command + missing_pkgs).ok()) {
                Logger::ok("依赖安装完成！");
            } else {
                Logger::error("依赖自动安装失败！请尝试手动执行包管理器命令。");
                return false;
            }
        } else {
            Logger::ok("基础依赖检查通过。");
        }
        return true;
    }

    bool Environment::set_locale(std::string_view loc) {
        // TODO: 配置 LANG / LC_ALL 环境变量
        return true;
    }

    void Environment::open_uri(const std::string &uri) const {
        if (cfg_.is_termux) {
            Logger::step("正在唤起 Android 意图 (Intent): " + uri);
            Executor::shell("am start -a android.intent.action.VIEW -d \"" + uri + "\" >/dev/null 2>&1");
        } else {
            Logger::step("正在唤起宿主机默认程序: " + uri);
            // 从 root 降权回真实用户以启动 UI 程序
            std::string sudo_user = std::getenv("SUDO_USER") ? std::getenv("SUDO_USER") : "";
            std::string cmd;
            if (!sudo_user.empty()) {
                cmd = "su - " + sudo_user + " -c \"xdg-open '" + uri + "'\"";
            } else {
                cmd = "xdg-open \"" + uri + "\"";
            }
            // 将打开器放入后台，避免阻塞容器生命周期
            Executor::shell(cmd + " >/dev/null 2>&1 &");
        }
    }
} // namespace tmoe::domain
