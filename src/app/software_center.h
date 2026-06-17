#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <string>
#include <vector>

namespace tmoe::app {

/** 软件中心 — 常用应用一键安装。
 *  对应 Bash: tools/app/center
 */
class SoftwareCenter {
public:
    explicit SoftwareCenter(const TmoeConfig& cfg) : cfg_(cfg) {}

    /** 列出可用软件包。 */
    [[nodiscard]] std::vector<std::string> available() const;

    /** 按名称安装软件包。 */
    bool install(std::string_view name);

    /** 按名称卸载软件包。 */
    bool remove(std::string_view name);

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::app
