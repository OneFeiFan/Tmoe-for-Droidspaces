#pragma once
#include "core/config.h"
#include <string>
#include <vector>

namespace tmoe::app {

/// 软件中心 — 一键安装常用软件
/// 对应 Bash: tools/app/center
class SoftwareCenter {
public:
    explicit SoftwareCenter(const TmoeConfig& cfg) : cfg_(cfg) {}

    /// 列出可安装的软件
    std::vector<std::string> available() const;

    /// 安装指定软件
    bool install(std::string_view name);

    /// 卸载
    bool remove(std::string_view name);

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::app
