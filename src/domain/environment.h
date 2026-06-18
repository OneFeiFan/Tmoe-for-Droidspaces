#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <string>

namespace tmoe::domain {
    /** 环境初始化与依赖检测。
     *  对应原版 Bash 脚本:
     *    share/environment/manager_environment
     *    share/environment/dependencies
     *    share/environment/locale
     */
    class Environment {
    public:
        explicit Environment(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /** 初始化 tmoe 工作环境（创建目录、基础检查）。 */
        bool initialize();

        /** 检查并安装缺失的系统依赖包（含 locale 基础设施与字体）。 */
        bool check_dependencies();

        /** 设置当前进程的 locale 环境变量并确保对应字体可用。
         *  自动生成缺失的 locale、安装缺失的字体；
         *  Debian 系在自动生成失败时提供 dpkg-reconfigure locales 回退。
         *  @note 仅影响当前进程及其子进程，重启后失效。
         */
        bool set_locale(std::string_view loc);

        /** 将 locale 写入系统配置文件，使其持久化（重启后仍生效）。
         *  优先使用 localectl (systemd)，回退到直接写 /etc/default/locale
         *  或 /etc/locale.conf。
         */
        bool apply_system_locale(std::string_view loc);

        /** 跨平台 URI 唤起器（处理 Android Intent 和 xdg-open）。 */
        void open_uri(const std::string &uri) const;

    private:
        const TmoeConfig &cfg_;

        // ── locale 相关 ──
        /** 检查指定 locale 是否已生成（locale -a）。 */
        bool is_locale_generated(std::string_view lang) const;

        /** 非交互式生成单个 locale（各发行版差异化实现）。 */
        bool generate_locale_auto(std::string_view lang);

        /** Debian 系: 交互式 dpkg-reconfigure locales（接管终端）。 */
        bool generate_locale_debian_interactive();

        /** Arch/Gentoo 系: 编辑 /etc/locale.gen 并执行 locale-gen。 */
        bool generate_locale_via_locale_gen(std::string_view lang);

        // ── 字体相关 ──
        /** 检查终端字体是否覆盖指定语言的字形（fc-list :lang=xx）。 */
        bool has_font_coverage(std::string_view lang) const;

        /** 按发行版安装对应的字体包。 */
        bool install_font_packages();

        /** 刷新 fontconfig 缓存。 */
        void refresh_font_cache() const;

        // ── 映射工具 ──
        /** 将 i18n lang 代码映射为 fontconfig lang 标签。 */
        static std::string lang_to_fc_code(std::string_view lang);

        /** 判断语言是否需要 CJK 字形支持。 */
        static bool needs_cjk(std::string_view lang);

        /** 判断语言是否需要西里尔字形支持。 */
        static bool needs_cyrillic(std::string_view lang);
    };
} // namespace tmoe::domain
