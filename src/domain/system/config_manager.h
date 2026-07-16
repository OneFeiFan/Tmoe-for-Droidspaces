#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include "domain/system/environment.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

namespace tmoe::domain {

/** 容器配置管理 — 对应原版 Bash `share/configuration/menu` 全部功能。
 *
 *  职责:
 *   1. DNS 切换 (6 种 DNS 提供商)
 *   2. 时区配置 (UTC-12 ~ UTC+14, 30 常用)
 *   3. Locale 选择 (300+ 语言, 分组浏览)
 *   4. Fortune/一言 (MOTD 欢迎语)
 *   5. 共享目录开关 (sd/HOME/storage/tf)
 *   6. 密码管理 (root 密码修改)
 *   7. 主机名配置
 */
class ConfigManager {
public:
    explicit ConfigManager(const TmoeConfig& cfg);

    // ── DNS ──

    /** DNS 选择器 TUI，返回选中的 DNS 提供商 ID。 */
    bool configure_dns();

    /** 将指定 DNS 写入 /etc/resolv.conf。 */
    bool apply_dns(const std::string& provider_id);

    // ── 时区 ──

    /** 时区选择器 TUI (分组浏览)。 */
    bool configure_timezone();

    /** 写入时区 (timedatectl 优先, 回退 ln -sf)。 */
    bool apply_timezone(const std::string& tz);

    /** 检测当前时区。 */
    std::string detect_current_timezone() const;

    // ── Locale ──

    /** 完整 Locale 选择器 (300+ 选项, 按区域分组)。 */
    bool configure_locale();

    /** 列出所有可用 locale (从 /usr/share/i18n/SUPPORTED 或内置表)。 */
    std::vector<std::pair<std::string, std::string>> list_supported_locales() const;

    // ── Fortune / Hitokoto ──

    /** Fortune (每日格言) 安装与配置。 */
    bool configure_fortune();

    /** 安装 fortune 包 (跨发行版)。 */
    bool install_fortune();

    /** 启用/禁用 Hitokoto (一言 API)。 */
    bool toggle_hitokoto();

    // ── 共享目录 ──

    /** 共享目录开关菜单。 */
    bool configure_shared_dirs();

    /** 写入挂载配置文件。 */
    bool write_mount_conf(const std::string& conf_name, bool enabled);

    // ── 密码 ──

    /** 修改 root 密码 (chpasswd)。 */
    bool change_root_password();

    // ── 主机名 ──

    /** 修改容器主机名。 */
    bool configure_hostname();

    /** 读取当前主机名。 */
    std::string current_hostname() const;

    /** 获取关联的 TmoeConfig 引用（供插件层构造依赖对象）。 */
    const TmoeConfig& config() const { return cfg_; }

    /** 获取 tmoe 配置目录路径。 */
    std::string config_dir() const;

    /** 写入配置文件（内部通过 SystemHelper 处理权限）。 */
    bool write_config_file(const std::string& path, const std::string& content) const;

    // ── DNS 注册表 ──
    struct DnsEntry {
        std::string id;
        std::string name_key;   // i18n key for display name
        std::string primary;
        std::string secondary;
    };
    static const std::vector<DnsEntry>& dns_registry();

    // ── 时区注册表 ──
    struct TzEntry {
        std::string zone;
        std::string name_key;   // i18n key for display name
    };
    static const std::vector<std::pair<std::string, std::vector<TzEntry>>>& tz_registry();

    // ── Locale 注册表 ──
    static const std::vector<std::pair<std::string, std::vector<std::string>>>& locale_registry();

private:
    const TmoeConfig& cfg_;

    std::string read_config_file(const std::string& path) const;
};

} // namespace tmoe::domain
