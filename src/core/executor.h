#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <initializer_list>
#include <optional>

namespace tmoe {

/// 外部命令执行结果
struct ExecResult {
    int exit_code = -1;
    std::string stdout_data;
    std::string stderr_data;

    explicit operator bool() const { return exit_code == 0; }
    bool ok() const { return exit_code == 0; }
};

/// 统一的外部命令调用封装
/// 替代 Bash 的 `cmd arg1 arg2 2>/dev/null` 等模式
class Executor {
public:
    /// 逐参数调用，自动转义（推荐）
    static ExecResult run(std::string_view bin,
                          std::initializer_list<std::string_view> args);

    /// Shell 字符串调用（需要拼接复杂管道时用）
    static ExecResult shell(std::string_view cmd);

    /// 检查命令是否存在
    static bool has(std::string_view bin);

    /// 带超时的调用（秒）
    static ExecResult run_timeout(int timeout_sec,
                                  std::string_view bin,
                                  std::initializer_list<std::string_view> args);

    /// 设置环境变量后调用
    static ExecResult run_with_env(
        const std::vector<std::pair<std::string, std::string>>& env,
        std::string_view bin,
        std::initializer_list<std::string_view> args);

    /// 传入 whiptail 的基础参数，在用户屏幕渲染图形并返回用户选中的标签
    static std::string tui_select(std::string_view whiptail_args);

    /// 全局进程提权与重载引擎
    /// 会直接使用 execvp 替换当前进程映像，永不返回
    [[noreturn]] static void escalate_privileges(int argc, char* argv[]);
};

} // namespace tmoe
