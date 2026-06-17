#include "domain/environment.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {
    bool Environment::initialize() {
        Logger::step("初始化 tmoes 工作环境");
        cfg_.ensure_dirs();
        // TODO: 设置临时文件、清理残留
        return true;
    }

    bool Environment::check_dependencies() {
        if (cfg_.is_termux) {
            // Android 环境的依赖由专属的 TermuxManager 接管
            return true;
        }

        Logger::step("检查宿主机系统级依赖...");
        std::string missing_pkgs = "";

        // 1. 检查 sudo (除了原生无 sudo 的 Alpine)
        if (!Executor::has("sudo") && cfg_.linux_distro != "alpine") {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-admin/sudo" : " sudo";
        }

        // 2. 检查 TUI 核心组件 whiptail / dialog
        if (!Executor::has("whiptail") && !Executor::has("dialog")) {
            if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
                missing_pkgs += (cfg_.linux_distro == "gentoo") ? " dev-libs/newt" : " libnewt";
            } else if (cfg_.linux_distro == "openwrt") {
                missing_pkgs += " whiptail dialog";
            } else {
                missing_pkgs += " whiptail";
            }
        }

        // 3. 检查基础网络工具
        if (!Executor::has("curl") && !Executor::has("wget")) {
            missing_pkgs += " curl wget";
        }

        // 如果检测到缺失，执行全自动静默安装
        if (!missing_pkgs.empty()) {
            Logger::warn("缺失核心依赖，正在调用包管理器自动安装: " + missing_pkgs);
            Logger::info("Command: " + cfg_.install_command + missing_pkgs);

            // 先尝试更新缓存 (例如 apt update)
            Executor::shell(cfg_.update_command);

            // 执行正式安装
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
        // TODO: 设置 LANG / LC_ALL 等
        return true;
    }

    void Environment::open_uri(const std::string& uri) const {
        if (cfg_.is_termux) {
            Logger::step("正在唤起 Android 意图 (Intent): " + uri);
            // Termux 下免 Root 直接通过 Activity Manager 唤起
            Executor::shell("am start -a android.intent.action.VIEW -d \"" + uri + "\" >/dev/null 2>&1");
        } else {
            Logger::step("正在唤起宿主机默认程序: " + uri);
            // 核心细节：因为当前进程是提权后的 Root，必须尝试降权回到真实用户去唤起 UI 程序
            std::string sudo_user = std::getenv("SUDO_USER") ? std::getenv("SUDO_USER") : "";
            std::string cmd;
            if (!sudo_user.empty()) {
                cmd = "su - " + sudo_user + " -c \"xdg-open '" + uri + "'\"";
            } else {
                cmd = "xdg-open \"" + uri + "\"";
            }
            // 放到后台执行，防止阻塞容器生命周期
            Executor::shell(cmd + " >/dev/null 2>&1 &");
        }
    }
} // namespace tmoe::domain
