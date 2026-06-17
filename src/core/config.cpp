#include "core/config.h"
#include "core/executor.h"
#include <cstdlib>
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace tmoe {

TmoeConfig TmoeConfig::detect() {
    TmoeConfig cfg;
#ifndef _WIN32
    cfg.is_root = (geteuid() == 0); // 核心修复：精确探测当前是否具备真实 Root 权限
#endif

    // ── 检测平台 ──
#ifdef TMOE_PLATFORM_LINUX
    cfg.kernel = "linux";
#endif

    // ── 检测 Termux ──
    // 1. 判断是否处于 Android Termux 环境下
    const char* termux = std::getenv("TERMUX_VERSION");
    if (termux) {
        cfg.is_termux = true;
        cfg.linux_distro = "Android";
        cfg.os_pretty_name = "Android (Termux)";
        cfg.update_command = "apt update";
        cfg.install_command = "apt install -y";
        cfg.remove_command = "apt purge -y";
        cfg.work_dir = "/data/data/com.termux/files/home/.local/share/tmoe";
        cfg.temp_dir = "/data/data/com.termux/files/usr/tmp/tmoe";
        cfg.container_root = cfg.work_dir / "containers";
        cfg.tui_bin = "whiptail"; // 稍后可在领域层判断是否启用 android 独有 wrapper
        return cfg; // Termux 探测直接安全截断返回
    }

    // 2. 判断是否属于 WSL 环境
    auto kernel_version = Executor::shell("uname -r").stdout_data;
    if (kernel_version.find("Microsoft") != std::string::npos ||
        kernel_version.find("microsoft") != std::string::npos) {
        cfg.is_wsl = true;
    }

    // 3. 安全读取 /etc/os-release，进行高内聚特征识别
    std::ifstream file("/etc/os-release");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 提取 PRETTY_NAME
    size_t pos = content.find("PRETTY_NAME=");
    if (pos != std::string::npos) {
        size_t start = content.find('"', pos);
        size_t end = content.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            cfg.os_pretty_name = content.substr(start + 1, end - start - 1);
        }
    }

    // 发行版矩阵精准状态映射 (对标原 Bash 判断)
    if (content.find("debian") != std::string::npos || content.find("ubuntu") != std::string::npos ||
        content.find("deepin") != std::string::npos || content.find("kali") != std::string::npos) {
        cfg.linux_distro = "debian";
        cfg.update_command = "apt update";
        cfg.install_command = "apt install -y";
        cfg.remove_command = "apt purge -y";
        if (content.find("ubuntu") != std::string::npos) cfg.sub_distro = "ubuntu";
        else if (content.find("kali") != std::string::npos) cfg.sub_distro = "kali";
    }
    else if (content.find("Arch") != std::string::npos || content.find("Manjaro") != std::string::npos) {
        cfg.linux_distro = "arch";
        cfg.update_command = "pacman -Syy";
        cfg.install_command = "pacman -Syu --noconfirm --needed";
        cfg.remove_command = "pacman -Rsc";
    }
    else if (content.find("Alpine") != std::string::npos) {
        cfg.linux_distro = "alpine";
        cfg.update_command = "apk update";
        cfg.install_command = "apk add";
        cfg.remove_command = "sudo apk del";
    }

    // 4. 判定默认的容器根目录存放路径
    cfg.container_root = "/var/lib/tmoe/containers";

    // 兜底支持从外界全局环境变量任意强行覆盖
    cfg.from_env();
    return cfg;
}

void TmoeConfig::from_env() {
    auto get = [](const char* name) -> const char* {
        return std::getenv(name);
    };

    if (auto* v = get("TMOE_WORK_DIR"))   work_dir = v;
    if (auto* v = get("TMOE_TEMP_DIR"))   temp_dir = v;
    if (auto* v = get("TMOE_ARCH"))       arch = v;
    if (auto* v = get("TMOE_LOCALE"))     locale = v;
    if (auto* v = get("TMOE_DISTRO"))     default_distro = v;
    if (auto* v = get("TMOE_MODE"))       default_mode = v;
    if (auto* v = get("TMOE_MIRROR"))     mirror_region = v;
    if (auto* v = get("TMOE_ROOT"))       is_root = (std::string(v) == "1");
}

void TmoeConfig::export_env() const {
    // 设置环境变量供子进程使用
    auto set = [](const char* k, const std::string& v) {
#ifdef _WIN32
        _putenv_s(k, v.c_str());
#else
        ::setenv(k, v.c_str(), 1);
#endif
    };
    set("TMOE_WORK_DIR",  work_dir.string());
    set("TMOE_TEMP_DIR",  temp_dir.string());
    set("TMOE_ARCH",      arch);
    set("TMOE_LOCALE",    locale);
    set("TMOE_DISTRO",    default_distro);
    set("TMOE_MODE",      default_mode);
    set("TMOE_MIRROR",    mirror_region);
}

void TmoeConfig::ensure_dirs() const {
    fs::create_directories(work_dir);
    fs::create_directories(temp_dir);
    fs::create_directories(container_root);
    fs::create_directories(backup_dir);
}

} // namespace tmoe
