#include "executor.h"
#include "i18n.h"


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
        // _pclose 在 Windows 上返回子进程的退出状态，格式为:
        //   高字节 = 退出码，低字节 = 0 (正常退出) 或 信号编号
        // 参考: MSDN _pclose / _cwait 文档
        int exit_code = (rc >= 0) ? ((rc >> 8) & 0xFF) : -1;
        return ExecResult{exit_code, std::move(output), ""};
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
        // _pclose 返回子进程退出状态，高字节=退出码
        // 参考: MSDN _pclose returns exit status of cmd.exe
        int shell_exit = (rc >= 0) ? ((rc >> 8) & 0xFF) : -1;
        return ExecResult{shell_exit, std::move(output), ""};
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
        std::string args_str(whiptail_args);
        std::string cmd;

        // dialog 和 whiptail 的 stdout/stderr 方向相反:
        //   whiptail: TUI→stdout, choice→stderr → 需要 3>&1 1>&2 2>&3 交换
        //   dialog:   TUI→stderr, choice→stdout → 加 --stdout 即可, 不需要交换
        bool is_dialog = (args_str.find("dialog") != std::string::npos &&
                          args_str.find("whiptail") == std::string::npos);

        if (is_dialog) {
            // dialog: 确保 --stdout 让选项输出到 stdout，popen 直接捕获
            if (args_str.find("--stdout") == std::string::npos) {
                size_t first_dash = args_str.find(" --");
                if (first_dash != std::string::npos) {
                    args_str.insert(first_dash, " --stdout");
                } else {
                    args_str += " --stdout";
                }
            }
            // 消除阴影: CJK 双宽度字符下阴影(░▒▓)会错位产生"像素凸起"
            if (args_str.find("--no-shadow") == std::string::npos) {
                size_t first_dash = args_str.find(" --");
                if (first_dash != std::string::npos) {
                    args_str.insert(first_dash, " --no-shadow");
                }
            }
            // 防止 CJK 空白被折叠导致宽度再计算
            if (args_str.find("--no-collapse") == std::string::npos) {
                size_t first_dash = args_str.find(" --");
                if (first_dash != std::string::npos) {
                    args_str.insert(first_dash, " --no-collapse");
                }
            }
            cmd = args_str; // TUI→stderr→终端, choice→stdout→pipe
        } else {
            // whiptail: 交换 stdout/stderr，使选项进入 pipe
            cmd = args_str + " 3>&1 1>&2 2>&3";
        }

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

    [[noreturn]] void Executor::escalate_privileges(int argc, char *argv[]) {
#ifndef _WIN32
    if (geteuid() == 0) {
        std::exit(0); // 已是 root —— 理论上不应到达此处
    }

    if (has("sudo")) {
        Logger::warn(_("exec.sudo_elevating"));

        std::vector<char*> exec_args;
        exec_args.push_back(const_cast<char*>("sudo"));
        exec_args.push_back(const_cast<char*>("-E")); // 保留环境变量

        for (int i = 0; i < argc; ++i) {
            exec_args.push_back(argv[i]);
        }
        exec_args.push_back(nullptr);

        ::execvp("sudo", exec_args.data());

        Logger::error(_("exec.sudo_failed"));
        std::exit(1);

    } else if (has("su")) {
        Logger::warn(_("exec.su_elevating"));

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

        Logger::error(_("exec.su_failed"));
        std::exit(1);
    } else {
        Logger::error(_("exec.no_su_sudo"));
        std::exit(1);
    }
#else
        Logger::error(_("exec.no_auto_elevate"));
        std::exit(1);
#endif
    }
} // namespace tmoe
