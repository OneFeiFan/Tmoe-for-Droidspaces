#pragma once
#include <cstdio>
#include <cstdlib>
#include <array>
#include <memory>
#include <stdexcept>
#include "logger.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif
#include <string>
#include <string_view>
#include <vector>
#include <initializer_list>
#include <optional>

namespace tmoe {
    /** 外部命令执行结果。 */
    struct ExecResult {
        int exit_code = -1;
        std::string stdout_data;
        std::string stderr_data;

        explicit operator bool() const { return exit_code == 0; }
        [[nodiscard]] bool ok() const { return exit_code == 0; }
    };

    /** 统一的外部命令调用封装。
     *  替代 Bash 中 `cmd arg1 arg2 2>/dev/null` 等模式。
     */
    class Executor {
    public:
        /** 逐参数调用，自动安全转义（推荐）。 */
        static ExecResult run(std::string_view bin,
                              std::initializer_list<std::string_view> args);

        /** 原始 Shell 字符串调用（用于复杂管道场景）。 */
        static ExecResult shell(std::string_view cmd);

        /** 检查命令是否在 PATH 中可用。 */
        static bool has(std::string_view bin);

        /** 带超时的调用（秒）。
         *  @note 当前为占位实现 — 回退到 run()。
         */
        static ExecResult run_timeout(int timeout_sec,
                                      std::string_view bin,
                                      std::initializer_list<std::string_view> args);

        /** 设置额外环境变量后调用。
         *  @note 当前为占位实现 — 回退到 run()。
         */
        static ExecResult run_with_env(
            const std::vector<std::pair<std::string, std::string> > &env,
            std::string_view bin,
            std::initializer_list<std::string_view> args);

        /** 启动 whiptail 对话框并返回用户选中的标签。 */
        static std::string tui_select(std::string_view whiptail_args);

        /** 直接透传终端给子进程（用于交互式命令）。
         *  底层使用 system() —— 子进程完全继承 stdin/stdout/stderr。
         *  适用于 dpkg-reconfigure、passwd、visudo 等需要终端交互的场景。
         */
        static ExecResult passthrough(std::string_view cmd);

        /** 通过 sudo/su 提升权限并替换当前进程。
         *  调用 execvp —— 永不返回。
         */
        [[noreturn]] static void escalate_privileges(int argc, char *argv[]);
    };
} // namespace tmoe
