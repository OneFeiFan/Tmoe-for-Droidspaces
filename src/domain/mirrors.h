#pragma once
#include "core/config.h"
#include "mirror_registry.h"
#include <string>
#include <vector>

namespace tmoe::domain {

/// 镜像源管理 — 对应 Bash `tools/sources/gnu-linux_mirrors` 的全部功能
///
/// 职责：
///  1. 发行版路由（Debian/Ubuntu/Kali/Arch/Manjaro/Alpine）
///  2. 旧源备份（~/.config/tmoe-linux/*.bak）
///  3. sources.list / mirrorlist / repositories 模板生成与写入
///  4. 发行版版本号自动检测（VERSION_CODENAME + fallback）
///  5. ARM 架构自动适配（ubuntu-ports / archlinuxarm）
///  6. 测速自动选择
///  7. 还原备份 / 手动编辑 / HTTP-HTTPS 切换 / 去重去注释 / 强制信任
class MirrorManager {
public:
    explicit MirrorManager(const TmoeConfig& cfg);

    // ── 基础查询 ──
    /// 列出当前发行版支持的镜像源
    std::vector<MirrorEntry> available() const;

    /// 按分类列出
    std::vector<MirrorEntry> by_category(const std::string& cat) const;

    // ── 核心操作 ──
    /// 切换到指定镜像源 (id = "tuna" / "ustc" / "aliyun" ...)
    bool switch_to(const std::string& mirror_id);

    /// ping 各大镜像站，自动选择延迟最低的
    bool auto_select();

    /// 从备份文件还原原始源
    bool restore_official();

    // ── 辅助操作 ──
    /// 手动编辑源列表文件
    bool edit_manually();

    /// HTTP ↔ HTTPS 互切
    bool toggle_http_https(bool use_https);

    /// 去除注释行 + 排序去重
    bool clean_sources_list();

    /// 强制信任软件源 (Debian: [trusted=yes], Arch: SigLevel=Never)
    bool trust_sources(bool trust);

    /// 发行版版本号检测
    std::string detect_version_codename() const;

private:
    const TmoeConfig& cfg_;

    // ── 内部写入引擎 ──
    bool write_debian_sources(const MirrorEntry& mirror);
    bool write_ubuntu_sources(const MirrorEntry& mirror);
    bool write_kali_sources(const MirrorEntry& mirror);
    bool write_arch_sources(const MirrorEntry& mirror);
    bool write_alpine_sources(const MirrorEntry& mirror);

    // ── 通用工具 ──
    /// 首次切换时备份原文件（备份只做一次）
    bool ensure_backup() const;
    /// 执行发行版更新命令 (apt update / pacman -Syyu / apk update)
    bool run_update() const;
    /// 执行发行版发行升级 (apt dist-upgrade / pacman -Syu / apk upgrade)
    bool run_dist_upgrade() const;
    /// 将 CA 证书路径从 http 替换为 https
    bool http_to_https_if_ca_available() const;
    /// DNS 预解析：从镜像 URL 中提取 hostname 并解析为 IP（getent → python3 fallback）
    std::string resolve_hostname(const std::string& url) const;
};

} // namespace tmoe::domain
