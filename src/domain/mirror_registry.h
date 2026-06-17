//
// Created by WorkBuddy on 2026/5/26.
//
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace tmoe::domain {

/// 单个镜像站的元数据（与 mirror_registry.json 中的结构一一对应）
struct MirrorEntry {
    std::string id;
    std::string name;
    std::string url;
    std::string category;   // "university" | "business" | "worldwide"
    std::string region;     // "cn" | "hk" | "tw" | "jp" | "us" ...
    std::string note;       // 可选备注（如 Debian 专用说明）
};

/// 发行版特定的源文件/备份路径元数据
struct MirrorCompatInfo {
    std::string source_file;
    std::string backup_path;
    std::string source_conf;        // 仅 Arch/pacman.conf 用到
    std::string backup_conf;
    std::string security_official;  // 仅 Debian 系用到
    std::string arm_repo;           // Arch/Manjaro: arm 架构仓名
    std::string x86_repo;           // Arch/Manjaro: x86 架构仓名
    std::string arm_path;           // Ubuntu: /ubuntu-ports
    std::string rolling_branch;     // Kali: kali-rolling
    std::string source_dir;         // RedHat: yum.repos.d 目录
    std::vector<std::string> repos; // Alpine: ["main", "community"]
};

/// 镜像源注册表 — 编译期注入 JSON，运行时按需查询
/// 参照 RootfsRegistry 的单例模式 + nlohmann::json 解析
class MirrorRegistry {
public:
    static MirrorRegistry& instance();

    MirrorRegistry(const MirrorRegistry&) = delete;
    MirrorRegistry& operator=(const MirrorRegistry&) = delete;

    // ── 镜像源查询 ──
    /// 按分类筛选镜像站列表
    std::vector<MirrorEntry> by_category(const std::string& category) const;

    /// 按地区筛选
    std::vector<MirrorEntry> by_region(const std::string& region) const;

    /// 全部镜像站
    std::vector<MirrorEntry> all() const;

    /// 按 id 查找
    std::optional<MirrorEntry> find(const std::string& id) const;

    /// 默认镜像 id
    std::string default_mirror() const;

    // ── 发行版兼容信息 ──
    std::optional<MirrorCompatInfo> compat_for(const std::string& distro) const;

    /// 从 MirrorEntry 拼接各发行版的实际源地址
    /// 例如 Debian 会产出: http://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm main contrib non-free
    static std::string build_source_url(const MirrorEntry& mirror,
                                        const std::string& distro,
                                        const std::string& codename);

private:
    MirrorRegistry();
    nlohmann::json data_;
};

} // namespace tmoe::domain
