#pragma once

#include "mirror_data.h"
#include <algorithm>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace tmoe::domain {

/** 单个镜像站点的元数据（与 mirror_registry.json 结构对应）。 */
    struct MirrorEntry {
        std::string id;
        std::string name;
        std::string url;
        std::string category;   // "university" | "business" | "worldwide"
        std::string region;     // "cn" | "hk" | "tw" | "jp" | "us" ...
        std::string note;
    };

/** 各发行版特定的源/备份路径元数据。 */
    struct MirrorCompatInfo {
        std::string source_file;
        std::string backup_path;
        std::string source_conf;        // Arch: pacman.conf
        std::string backup_conf;
        std::string security_official;  // Debian 家族
        std::string arm_repo;           // Arch/Manjaro: ARM 仓库名
        std::string x86_repo;           // Arch/Manjaro: x86 仓库名
        std::string arm_path;           // Ubuntu: /ubuntu-ports
        std::string rolling_branch;     // Kali: kali-rolling
        std::string source_dir;         // RedHat: yum.repos.d 目录
        std::vector<std::string> repos; // Alpine: ["main", "community"]
    };

/** 镜像注册表 — 编译期嵌入 JSON，运行时查询。
 *  沿用与 RootfsRegistry 相同的单例 + nlohmann::json 模式。
 */
    class MirrorRegistry {
    public:
        static MirrorRegistry &instance();

        MirrorRegistry(const MirrorRegistry &) = delete;

        MirrorRegistry &operator=(const MirrorRegistry &) = delete;

        // ── 镜像查询 ──

        /** 按分类筛选镜像。 */
        std::vector<MirrorEntry> by_category(const std::string &category) const;

        /** 按地区筛选镜像。 */
        std::vector<MirrorEntry> by_region(const std::string &region) const;

        /** 获取所有已注册的镜像。 */
        std::vector<MirrorEntry> all() const;

        /** 按 id 查找镜像。 */
        std::optional<MirrorEntry> find(const std::string &id) const;

        /** 获取默认镜像 id。 */
        std::string default_mirror() const;

        // ── 发行版兼容信息 ──

        /** 获取指定发行版的兼容性元数据。 */
        std::optional<MirrorCompatInfo> compat_for(const std::string &distro) const;

        /** 构建发行版实际使用的源 URL。
         *  例如 Debian: http://mirrors.tuna.tsinghua.edu.cn/debian/ bookworm main contrib non-free
         */
        static std::string build_source_url(const MirrorEntry &mirror,
                                            const std::string &distro,
                                            const std::string &codename);

    private:
        MirrorRegistry();

        nlohmann::json data_;
    };

} // namespace tmoe::domain
