#include "executor.h"
#include "i18n.h"

#ifndef _WIN32
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <chrono>

namespace tmoe {
    std::string shell_escape(std::string_view arg) {
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

    /** popen → 读取全部输出 → pclose → ExecResult。
     *  消除 run / shell / run_with_env 中重复的管道管理代码。 */
    static ExecResult popen_exec(const std::string& cmd) {
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
        // _pclose 返回子进程退出状态，高字节=退出码
        int exit_code = (rc >= 0) ? ((rc >> 8) & 0xFF) : -1;
        return ExecResult{exit_code, std::move(output), ""};
#else
        return ExecResult{WIFEXITED(rc) ? WEXITSTATUS(rc) : -1, std::move(output), ""};
#endif
    }

    ExecResult Executor::run(std::string_view bin,
                             std::initializer_list<std::string_view> args) {
        std::string cmd(bin);
        for (auto &a: args) {
            cmd += " ";
            cmd += shell_escape(a);
        }
        cmd += " 2>&1";
        return popen_exec(cmd);
    }

    ExecResult Executor::shell(std::string_view cmd) {
        std::string full_cmd(cmd);
        // heredoc 结束标记必须独占一行，追加 2>&1 会破坏 <<-'EOF' 的识别
        // → TMOE_EOF 被写入文件正文 → apt 报 "无法识别类别 TMOE_EOF"
        if (full_cmd.find("<<-") == std::string::npos) {
            full_cmd += " 2>&1";
        }

        return popen_exec(full_cmd);
    }

    bool Executor::has(std::string_view bin) {
        auto result = shell(std::string("command -v ") + std::string(bin) + " >/dev/null 2>&1");
        return result.exit_code == 0;
    }

    ExecResult Executor::run_timeout(int timeout_sec,
                                     std::string_view bin,
                                     std::initializer_list<std::string_view> args) {
        if (timeout_sec <= 0) {
            return run(bin, args);
        }

#ifndef _WIN32
        // 构建与 run() 相同的命令字符串
        std::string cmd(bin);
        for (const auto& a : args) {
            cmd += " ";
            cmd += shell_escape(a);
        }
        cmd += " 2>&1";

        int pipefd[2];
        if (::pipe(pipefd) == -1) {
            return ExecResult{-1, "", "pipe() failed"};
        }

        pid_t pid = ::fork();
        if (pid == -1) {
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            return ExecResult{-1, "", "fork() failed"};
        }

        if (pid == 0) {
            // 子进程: 重定向 stdout/stderr 到管道，通过 sh -c 执行
            ::close(pipefd[0]);
            ::dup2(pipefd[1], STDOUT_FILENO);
            ::dup2(pipefd[1], STDERR_FILENO);
            ::close(pipefd[1]);
            ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            ::_exit(127);
        }

        // 父进程: 带截止时间的 select 轮询读取
        ::close(pipefd[1]);

        std::string output;
        std::array<char, 4096> buf;
        auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(timeout_sec);
        bool timed_out = false;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                timed_out = true;
                break;
            }

            auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now);

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(pipefd[0], &fds);
            struct timeval tv;
            tv.tv_sec  = remaining_ms.count() / 1000;
            tv.tv_usec = (remaining_ms.count() % 1000) * 1000;

            int ret = ::select(pipefd[0] + 1, &fds, nullptr, nullptr, &tv);
            if (ret == -1) {
                if (errno == EINTR) continue;
                break;
            }
            if (ret == 0) {
                // select 自身超时 → 整体超时
                timed_out = true;
                break;
            }

            ssize_t n = ::read(pipefd[0], buf.data(), buf.size());
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;  // EOF: 子进程已退出
            output.append(buf.data(), n);
        }

        if (timed_out) {
            // 先 SIGTERM 优雅终止，500ms 后仍存活则 SIGKILL
            ::kill(pid, SIGTERM);
            ::usleep(500000);
            int status;
            if (::waitpid(pid, &status, WNOHANG) == 0) {
                ::kill(pid, SIGKILL);
                ::waitpid(pid, &status, 0);
            }
            ::close(pipefd[0]);
            return ExecResult{-1, std::move(output),
                "timeout: command exceeded " + std::to_string(timeout_sec) + "s"};
        }

        ::close(pipefd[0]);
        int status;
        ::waitpid(pid, &status, 0);
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return ExecResult{exit_code, std::move(output), ""};
#else
        // Windows: CreateProcess + WaitForSingleObject 实现真正超时
        std::string cmd(bin);
        for (const auto& a : args) {
            cmd += " ";
            cmd += shell_escape(a);
        }
        cmd += " 2>&1";
        std::string cmd_line = "cmd.exe /c " + cmd;

        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return ExecResult{-1, "", "CreatePipe failed"};
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {sizeof(STARTUPINFOA)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError  = hWritePipe;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = {0};
        std::vector<char> cmd_buf(cmd_line.c_str(),
                                   cmd_line.c_str() + cmd_line.size() + 1);

        if (!CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return ExecResult{-1, "", "CreateProcess failed"};
        }
        CloseHandle(hWritePipe);

        std::string output;
        std::array<char, 4096> buf;
        DWORD timeout_ms = static_cast<DWORD>(timeout_sec) * 1000;
        auto start_time = std::chrono::steady_clock::now();
        bool timed_out = false;

        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (static_cast<DWORD>(elapsed) >= timeout_ms) {
                timed_out = true;
                break;
            }

            DWORD remain  = timeout_ms - static_cast<DWORD>(elapsed);
            DWORD wait_ms = (remain < 100) ? remain : 100;

            DWORD wait_result = WaitForSingleObject(pi.hProcess, wait_ms);

            // 非阻塞读取所有可用管道数据
            DWORD available = 0;
            while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &available, NULL)
                   && available > 0) {
                DWORD bytes_read = 0;
                DWORD to_read = (available < static_cast<DWORD>(buf.size()))
                                    ? available
                                    : static_cast<DWORD>(buf.size());
                if (!ReadFile(hReadPipe, buf.data(), to_read, &bytes_read, NULL)
                    || bytes_read == 0) {
                    break;
                }
                output.append(buf.data(), bytes_read);
            }

            if (wait_result == WAIT_OBJECT_0) {
                break;
            }
        }

        if (timed_out) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            // 提取残留管道数据
            DWORD available = 0;
            if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &available, NULL)
                && available > 0) {
                DWORD bytes_read = 0;
                DWORD to_read = (available < static_cast<DWORD>(buf.size()))
                                    ? available
                                    : static_cast<DWORD>(buf.size());
                ReadFile(hReadPipe, buf.data(), to_read, &bytes_read, NULL);
                output.append(buf.data(), bytes_read);
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            return ExecResult{-1, std::move(output),
                "timeout: command exceeded " + std::to_string(timeout_sec) + "s"};
        }

        // 进程正常退出：排空所有残留数据
        DWORD available = 0;
        while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &available, NULL)
               && available > 0) {
            DWORD bytes_read = 0;
            DWORD to_read = (available < static_cast<DWORD>(buf.size()))
                                ? available
                                : static_cast<DWORD>(buf.size());
            if (!ReadFile(hReadPipe, buf.data(), to_read, &bytes_read, NULL)
                || bytes_read == 0) {
                break;
            }
            output.append(buf.data(), bytes_read);
        }

        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        return ExecResult{static_cast<int>(exit_code), std::move(output), ""};
#endif
    }

    ExecResult Executor::run_with_env(
        const std::vector<std::pair<std::string, std::string> > &env,
        std::string_view bin,
        std::initializer_list<std::string_view> args) {
        // 构建 "KEY1='val1' KEY2='val2' bin arg1 arg2 ... 2>&1"
        // shell 的 VAR=val 前缀语法会在执行命令前设置环境变量
        std::string cmd;
        for (const auto& [key, val] : env) {
            cmd += key;
            cmd += "=";
            cmd += shell_escape(val);
            cmd += " ";
        }
        cmd += bin;
        for (const auto& a : args) {
            cmd += " ";
            cmd += shell_escape(a);
        }
        cmd += " 2>&1";
        return popen_exec(cmd);
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

    std::string Executor::tui_select(std::string_view whiptail_args, bool& cancelled) {
        std::string args_str(whiptail_args);
        std::string cmd;
        bool is_dialog = (args_str.find("dialog") != std::string::npos &&
                          args_str.find("whiptail") == std::string::npos);

        if (is_dialog) {
            if (args_str.find("--stdout") == std::string::npos) {
                size_t first_dash = args_str.find(" --");
                if (first_dash != std::string::npos)
                    args_str.insert(first_dash, " --stdout");
                else
                    args_str += " --stdout";
            }
            if (args_str.find("--no-shadow") == std::string::npos) {
                size_t fd = args_str.find(" --");
                if (fd != std::string::npos) args_str.insert(fd, " --no-shadow");
            }
            if (args_str.find("--no-collapse") == std::string::npos) {
                size_t fd = args_str.find(" --");
                if (fd != std::string::npos) args_str.insert(fd, " --no-collapse");
            }
            cmd = args_str;
        } else {
            cmd = args_str + " 3>&1 1>&2 2>&3";
        }

        std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(cmd.c_str(), "r"), ::pclose);
        if (!pipe) { cancelled = true; return ""; }

        std::string result;
        try {
            std::array<char, 512> buf;
            while (std::fgets(buf.data(), buf.size(), pipe.get()))
                result += buf.data();
        } catch (...) { cancelled = true; return ""; }

        int rc = ::pclose(pipe.release());
#ifdef _WIN32
        int exit_code = (rc >= 0) ? ((rc >> 8) & 0xFF) : -1;
#else
        int exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#endif
        cancelled = (exit_code != 0);

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
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
        // 不用 -E：保留 $HOME 会导致 root 进程在用户 home 目录创建 root 归属文件
        // sudo 默认设 HOME=/root，$SUDO_USER 保留原始用户名，需要用户路径时用 getent 反查

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
