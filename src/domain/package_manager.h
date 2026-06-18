#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace tmoe::domain {

/** 发行版家族枚举 */
enum class DistroFamily {
    Debian,   // apt/eatmydata
    Arch,     // pacman
    RedHat,   // dnf/yum
    Alpine,   // apk
    Gentoo,   // emerge
    Suse,     // zypper
    Solus,    // eopkg
    Void_,    // xbps
    Slackware,// slackpkg
    OpenWrt,  // opkg
    Unknown
};

/** 跨发行版包管理器抽象 */
class PackageManager {
public:
    struct Commands {
        std::string install;   // 安装命令模板
        std::string remove;    // 移除命令模板
        std::string update;    // 更新索引命令模板
        bool use_eatmydata = false; // Debian 下使用 eatmydata 加速
    };

    static DistroFamily detect_distro_family();
    static Commands commands_for(DistroFamily family);

    /** 便捷安装 */
    static bool install(std::string_view pkg, DistroFamily family);
    static bool install(const std::vector<std::string>& pkgs, DistroFamily family);

    /** 便捷卸载 */
    static bool remove(std::string_view pkg, DistroFamily family);
    static bool remove(const std::vector<std::string>& pkgs, DistroFamily family);

    /** 更新索引 */
    static bool update(DistroFamily family);

    /** 检查命令是否可用 */
    static bool is_command_available(std::string_view cmd);

    /** 安装 eatmydata (仅 Debian 需要) */
    static bool ensure_eatmydata();

private:
    static std::string build_install_cmd(const std::vector<std::string>& pkgs,
                                         const Commands& cmd);
    static std::string build_remove_cmd(const std::vector<std::string>& pkgs,
                                        const Commands& cmd);
    static bool exec_with_fallback(const std::string& primary_cmd,
                                   const std::string& fallback_cmd);
};

/** 从 TmoeConfig 推断发行版家族 (供容器内使用) */
DistroFamily infer_family_from_config(const std::string& distro_name);

} // namespace tmoe::domain
#endif
