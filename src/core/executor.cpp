#include "executor.h"


namespace tmoe {

/** 对字符串进行单引号 Shell 转义。 */
static std::string shell_escape(std::string_view arg) {
    if (arg.empty()) return "''";
    std::string escaped = "'";
    for (char c: arg) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

/** 从 FILE* 读取全部数据直到 EOF。 */
static std::string read_all(FILE *fp) {
    std::string result;
    std::array<char, 4096> buf;
    while (std::fgets(buf.data(), buf.size(), fp)) {
        result += buf.data();
    }
    return result;
}

ExecResult Executor::run(std::string_view bin,
                         std::initializer_list<std::string_view> args) {
    std::string cmd(bin);
    for (auto &a: args) {
        cmd += " ";
        cmd += shell_escape(a);
    }
    cmd += " 2>&1";

    // unique_ptr 在作用域退出时自动关闭管道（包括异常情况）
    std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(cmd.c_str(), "r"), ::pclose);
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

ExecResult Executor::shell(std::string_view cmd) {
    std::string full_cmd(cmd);
    // heredoc 结束标记必须独占一行，追加 2>&1 会破坏 <<-'EOF' 的识别
    // → TMOE_EOF 被写入文件正文 → apt 报 "无法识别类别 TMOE_EOF"
    if (full_cmd.find("<<-") == std::string::npos) {
        full_cmd += " 2>&1";
    }

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

bool Executor::has(std::string_view bin) {
    auto result = shell(std::string("command -v ") + std::string(bin) + " >/dev/null 2>&1");
    return result.exit_code == 0;
}

ExecResult Executor::run_timeout(int timeout_sec,
                                 std::string_view bin,
                                 std::initializer_list<std::string_view> args) {
    // TODO: 通过 alarm/signal 或 std::thread + wait_for 实现真正的超时机制
    return run(bin, args);
}

ExecResult Executor::run_with_env(
    const std::vector<std::pair<std::string, std::string> > &env,
    std::string_view bin,
    std::initializer_list<std::string_view> args) {
    // TODO: 在 fork+exec 前设置环境变量
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

    // 去除尾部换行符
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

ExecResult Executor::passthrough(std::string_view cmd) {
    std::string full_cmd(cmd);
    int raw = std::system(full_cmd.c_str());
#ifdef _WIN32
    return ExecResult{raw, "", ""};
#else
    if (raw == -1) {
        return ExecResult{-1, "", "system() failed"};
    }
    return ExecResult{
        WIFEXITED(raw) ? WEXITSTATUS(raw) : -1,
        "",
        WIFSIGNALED(raw) ? ("signal " + std::to_string(WTERMSIG(raw))) : ""
    };
#endif
}

[[noreturn]] void Executor::escalate_privileges(int argc, char* argv[]) {
#ifndef _WIN32
    if (geteuid() == 0) {
        std::exit(0); // 已是 root —— 理论上不应到达此处
    }

    if (has("sudo")) {
        Logger::warn("This operation requires root privileges. Elevating via sudo...");

        std::vector<char*> exec_args;
        exec_args.push_back(const_cast<char*>("sudo"));
        exec_args.push_back(const_cast<char*>("-E")); // 保留环境变量

        for (int i = 0; i < argc; ++i) {
            exec_args.push_back(argv[i]);
        }
        exec_args.push_back(nullptr);

        ::execvp("sudo", exec_args.data());

        Logger::error("Failed to elevate privileges via sudo!");
        std::exit(1);

    } else if (has("su")) {
        Logger::warn("sudo not found. Elevating via su. Please enter root password...");

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

        Logger::error("Failed to elevate privileges via su!");
        std::exit(1);
    } else {
        Logger::error("Root privileges required, but neither sudo nor su was found!");
        std::exit(1);
    }
#else
    Logger::error("Current platform does not support automatic privilege elevation");
    std::exit(1);
#endif
}
} // namespace tmoe
