#pragma once
#include "core/executor.h"
#include "core/platform.h"
#include <cstdlib>
#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe {
    /** 发行版家族枚举。
     *  统一表示 Linux 发行版家族，替代裸字符串比较。 */
    enum class DistroFamily {
        Unknown = 0, // 默认值（零初始化安全）
        Debian,      // apt/eatmydata
        Arch,        // pacman
        RedHat,      // dnf/yum
        Alpine,      // apk
        Gentoo,      // emerge
        Suse,        // zypper
        Solus,       // eopkg
        Void_,       // xbps
        Slackware,   // slackpkg
        OpenWrt,     // opkg
    };

    /** 全局配置 — 替代 Bash 中所有 $TMOE_XXX 全局变量。 */
    struct TmoeConfig {
        // ── 路径 ──
        fs::path work_dir = "/data/local/tmoe";
        fs::path temp_dir = "/tmp/tmoe";
        fs::path backup_dir = "/sdcard/tmoe-backup";
        fs::path container_root; // detect() 时自动设置

        // ── 系统信息 ──
        std::string arch = "arm64"; // arm64 / amd64 / i386
        std::string kernel = "linux"; // linux / android
        std::string locale = "zh_CN.UTF-8";
        bool is_termux = false;
        bool is_root = false;
        bool is_chroot = false;   // true = 运行在 chroot/proot 容器内

        // ── 容器默认值 ──
        std::string default_distro = "debian";
        std::string default_mode = "proot"; // proot / chroot / nspawn
        bool privileged = true;

        // ── 镜像源 ──
        std::string mirror_region = "auto"; // cn / auto

        std::string linux_distro = "unknown";  // 发行版家族字符串（debian/arch/alpine…）
        DistroFamily distro_family = DistroFamily::Unknown;  // 类型化发行版家族
        std::string sub_distro = "unknown";
        std::string update_command = "apt update";
        std::string install_command = "apt install -y";
        std::string remove_command = "apt purge -y";
        std::string tui_bin = "whiptail";
        std::string os_pretty_name = "GNU/Linux";
        bool is_wsl = false;

        // ── 工厂方法 ──
        /** 从当前运行环境自动检测所有字段。 */
        static TmoeConfig detect();

        /** 从环境变量 (TMOE_*) 覆盖字段。 */
        void from_env();

        /** 将关键字段导出为环境变量供子进程使用。 */
        void export_env() const;

        /** 确保工作目录存在于磁盘上。 */
        void ensure_dirs() const;
    };
} // namespace tmoe
