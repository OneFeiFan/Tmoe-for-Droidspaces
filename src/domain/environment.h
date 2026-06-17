#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

/// 环境初始化 & 依赖检测
/// 对应 Bash: share/environment/manager_environment,
///           share/environment/dependencies,
///           share/environment/locale
class Environment {
public:
    explicit Environment(const TmoeConfig& cfg) : cfg_(cfg) {}

    /// 初始化 tmoe 工作环境（创建目录、检查依赖）
    bool initialize();

    /// 检查并安装缺失的系统依赖包
    bool check_dependencies();

    /// 设置系统 locale
    bool set_locale(std::string_view loc);

    /// 跨平台统一资源唤起接口 (处理 Intent 和 xdg-open)
    void open_uri(const std::string& uri) const;

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
