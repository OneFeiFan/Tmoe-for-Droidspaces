#pragma once
#include "core/config.h"
#include "mirror_registry.h"
#include "core/executor.h"
#include "core/logger.h"
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

namespace tmoe::domain {

/** 镜像源管理 — 涵盖原版 Bash `tools/sources/gnu-linux_mirrors` 脚本的全部功能。
 *
 *  职责:
 *   1. 发行版路由 (Debian/Ubuntu/Kali/Arch/Manjaro/Alpine)
 *   2. 原始源备份 (~/.config/tmoe-linux/*.bak)
 *   3. sources.list / mirrorlist / repositories 模板生成
 *   4. 版本代号自动检测 (VERSION_CODENAME + 回退方案)
 *   5. ARM 架构自动适配 (ubuntu-ports / archlinuxarm)
 *   6. 基于延迟的自动选择
 *   7. 备份还原 / 手动编辑 / HTTP-HTTPS 切换 / 去重 / 信任
 */
class MirrorManager {
public:
    explicit MirrorManager(const TmoeConfig& cfg);

    // ── 查询 ──

    /** 列出所有已注册的镜像源。 */
    std::vector<MirrorEntry> available() const;

    /** 按分类列出镜像源。 */
    std::vector<MirrorEntry> by_category(const std::string& cat) const;

    // ── 核心操作 ──

    /** 切换到指定镜像源 (id 例如 "tuna", "ustc", "aliyun")。 */
    bool switch_to(const std::string& mirror_id);

    /** Ping 所有镜像站点并自动选择最快的。 */
    bool auto_select();

    /** 从备份还原原始源。 */
    bool restore_official();

    // ── 辅助操作 ──

    /** 在文本编辑器中打开源列表文件。 */
    bool edit_manually();

    /** 切换 HTTP 和 HTTPS。 */
    bool toggle_http_https(bool use_https);

    /** 删除注释行并去重。 */
    bool clean_sources_list();

    /** 强制信任源 (Debian: [trusted=yes], Arch: SigLevel=Never)。 */
    bool trust_sources(bool trust);

    /** 检测当前发行版的版本代号。 */
    std::string detect_version_codename() const;

private:
    const TmoeConfig& cfg_;

    // ── 各发行版源写入器 ──
    bool write_debian_sources(const MirrorEntry& mirror);
    bool write_ubuntu_sources(const MirrorEntry& mirror);
    bool write_kali_sources(const MirrorEntry& mirror);
    bool write_arch_sources(const MirrorEntry& mirror);
    bool write_alpine_sources(const MirrorEntry& mirror);

    // ── 工具方法 ──
    /** 在首次切换时备份原始源（一次性操作）。 */
    bool ensure_backup() const;
    /** 运行发行版的 update 命令。 */
    bool run_update() const;
    /** 运行发行版的 dist-upgrade 命令。 */
    bool run_dist_upgrade() const;
    /** 如已安装 ca-certificates 则将 http:// 替换为 https://。 */
    bool http_to_https_if_ca_available() const;
    /** 将镜像 URL 的主机名解析为 IP（getent → python3 回退）。 */
    std::string resolve_hostname(const std::string& url) const;
};

} // namespace tmoe::domain
