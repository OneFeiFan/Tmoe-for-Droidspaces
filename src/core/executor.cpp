#include "core/executor.h"
#include <cstdio>
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

namespace tmoe {
    static std::string shell_escape(std::string_view arg) {
        if (arg.empty()) return "''";
        std::string escaped = "'";
        for (char c: arg) {
            if (c == '\'') {
                escaped += "'\\''"; // 将单引号安全闭合再重新开启
            } else {
                escaped += c;
            }
        }
        escaped += "'";
        return escaped;
    }

    static std::string read_all(FILE *fp) {
        std::string result;
        std::array<char, 4096> buf;
        while (std::fgets(buf.data(), buf.size(), fp)) {
            result += buf.data();
        }
        return result;
    }

    // ── run ──────────
    ExecResult Executor::run(std::string_view bin,
                             std::initializer_list<std::string_view> args) {
        std::string cmd(bin);
        for (auto &a: args) {
            cmd += " ";
            cmd += shell_escape(a); // 使用转义，现在传含空格的路径也绝对安全了
        }
        cmd += " 2>&1"; // stderr → stdout

        // 智能指针接管 FILE*，即使中途崩溃也能自动回收僵尸进程
        std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(cmd.c_str(), "r"), ::pclose);
        if (!pipe) {
            return ExecResult{-1, "", "popen failed"};
        }

        std::string output;
        try {
            output = read_all(pipe.get());
        } catch (...) {
            return ExecResult{-1, "", "read_all threw exception"}; // 异常时智能指针会自动 pclose
        }

        // 正常执行完毕，手动提取退出码并释放控制权
        int rc = ::pclose(pipe.release());

#ifdef _WIN32
        return ExecResult{rc, std::move(output), ""};
#else
        return ExecResult{WIFEXITED(rc) ? WEXITSTATUS(rc) : -1, std::move(output), ""};
#endif
    }

    // ── 替换现有的 shell 方法 ────────
    ExecResult Executor::shell(std::string_view cmd) {
        std::string full_cmd(cmd);
        full_cmd += " 2>&1";

        std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(full_cmd.c_str(), "r"), ::pclose);
        if (!pipe) {
            return ExecResult{-1, "", "popen failed"};
        }

        std::string output;
        try {
            output = read_all(pipe.get());
        } catch (...) {
            return ExecResult{-1, "", "read_all threw exception"};
        }

        int rc = ::pclose(pipe.release());

#ifdef _WIN32
        return ExecResult{rc, std::move(output), ""};
#else
        return ExecResult{WIFEXITED(rc) ? WEXITSTATUS(rc) : -1, std::move(output), ""};
#endif
    }

    // ── shell ────────


    // ── has ──────────
    bool Executor::has(std::string_view bin) {
        auto result = shell(std::string("command -v ") + std::string(bin) + " >/dev/null 2>&1");
        return result.exit_code == 0;
    }

    // ── run_timeout ──
    ExecResult Executor::run_timeout(int timeout_sec,
                                     std::string_view bin,
                                     std::initializer_list<std::string_view> args) {
        // TODO: 用 alarm/signal 实现超时，或用 std::thread + wait_for
        // 当前占位：直接调用 run
        return run(bin, args);
    }

    // ── run_with_env ─
    ExecResult Executor::run_with_env(
        const std::vector<std::pair<std::string, std::string> > &env,
        std::string_view bin,
        std::initializer_list<std::string_view> args) {
        // TODO: 在 fork+exec 前设置环境变量
        // 当前占位：直接调用 run
        return run(bin, args);
    }

    std::string Executor::tui_select(std::string_view whiptail_args) {
        std::string cmd = std::string(whiptail_args) + " 3>&1 1>&2 2>&3";

        std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(cmd.c_str(), "r"), ::pclose);
        if (!pipe) {
            return "";
        }

        std::string result;
        try {
            std::array<char, 512> buf;
            while (std::fgets(buf.data(), buf.size(), pipe.get())) {
                result += buf.data();
            }
        } catch (...) {
            return "";
        }

        ::pclose(pipe.release());

        // 去除返回的尾部换行符
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }

    [[noreturn]] void Executor::escalate_privileges(int argc, char* argv[]) {
#ifndef _WIN32
        if (geteuid() == 0) {
            std::exit(0); // 已经是 root，理论上不该触发
        }

        if (has("sudo")) {
            Logger::warn("当前操作需要 Root 权限，正在通过 sudo 提权...");

            std::vector<char*> exec_args;
            exec_args.push_back(const_cast<char*>("sudo"));
            exec_args.push_back(const_cast<char*>("-E")); // 原版细节：必须保留环境变量

            // 将原始参数原封不动透传
            for (int i = 0; i < argc; ++i) {
                exec_args.push_back(argv[i]);
            }
            exec_args.push_back(nullptr);

            // 直接替换当前进程
            ::execvp("sudo", exec_args.data());

            Logger::error("sudo 进程提权拉起失败！");
            std::exit(1);

        } else if (has("su")) {
            Logger::warn("未检测到 sudo，正在通过 su 提权，请输入 root 密码...");

            // 构建安全的带转义单行命令，用于 su -c
            std::string cmd = argv[0];
            for (int i = 1; i < argc; ++i) {
                cmd += " ";
                cmd += shell_escape(argv[i]);
            }

            std::vector<char*> exec_args;
            exec_args.push_back(const_cast<char*>("su"));
            exec_args.push_back(const_cast<char*>("-c"));
            exec_args.push_back(const_cast<char*>(cmd.c_str()));
            exec_args.push_back(nullptr);

            ::execvp("su", exec_args.data());

            Logger::error("su 进程提权拉起失败！");
            std::exit(1);
        } else {
            Logger::error("需要 Root 权限，但在系统中未找到 sudo 或 su 命令！");
            std::exit(1);
        }
#else
        Logger::error("当前平台不支持自动权限提升");
        std::exit(1);
#endif
    }
} // namespace tmoe
