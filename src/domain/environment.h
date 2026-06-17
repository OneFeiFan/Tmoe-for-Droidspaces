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

        /** 检查并安装缺失的系统依赖包。 */
        bool check_dependencies();

        /** 设置系统 locale。
         *  @note 当前为占位实现 — TODO: 实现 LANG/LC_ALL 配置。
         */
        bool set_locale(std::string_view loc);

        /** 跨平台 URI 唤起器（处理 Android Intent 和 xdg-open）。 */
        void open_uri(const std::string &uri) const;

    private:
        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
