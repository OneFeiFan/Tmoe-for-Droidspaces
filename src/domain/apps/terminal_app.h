#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H
#pragma once
#include <string>
#include <vector>
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

class TerminalAppManager {
public:
    explicit TerminalAppManager(const TmoeConfig& cfg);
    void run_terminal_menu();

    /** 安装单个终端应用（含步骤提示和安装后提示）。
     *  供 UI 插件 LambdaAction 调用，替代旧 whiptail 循环中的内联逻辑。 */
    void install_terminal(const std::string& pkg,
                          const std::string& label_key,
                          const std::string& hint_key = "");

private:
    const TmoeConfig& cfg_;
    DistroFamily family_;
};

} // namespace tmoe::domain
#endif
