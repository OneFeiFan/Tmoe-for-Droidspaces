#ifndef PLATFORM_H
#define PLATFORM_H
#pragma once
/**
 * 跨平台抽象层。
 * 统一提供 Linux/Windows 之间差异的系统调用 polyfill，
 * 避免各 .cpp 文件各自 #ifdef _WIN32 定义相同的桩函数。
 */

#include <cstdio>       // FILE*, popen, pclose (Linux), _popen/_pclose (Windows)
#include <string_view>

#ifdef _WIN32
// ── Windows 编译环境 ──
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <io.h>         // _popen, _pclose

#else
// ── Linux / WSL / Termux 原生环境 ──
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>   // WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG
#endif

namespace tmoe::platform {

// ═══════════════════════════════════════════════════════════
// 进程/用户身份
// ═══════════════════════════════════════════════════════════

#ifdef _WIN32
inline int getpid() { return ::_getpid(); }
inline int getuid() { return 0; }  // Windows 无 UID 概念
inline int geteuid() { return 0; } // Windows 无有效 UID 概念
inline bool is_root() { return false; } // Windows 下近似（实际由 UAC 控制，但编译通过优先）
#else
inline int getpid() { return ::getpid(); }
inline int getuid() { return ::getuid(); }
inline int geteuid() { return ::geteuid(); }
inline bool is_root() { return ::geteuid() == 0; }
#endif

// ═══════════════════════════════════════════════════════════
// 进程管理
// ═══════════════════════════════════════════════════════════

inline FILE* popen(const char* cmd, const char* mode) {
#ifdef _WIN32
    return ::_popen(cmd, mode);
#else
    return ::popen(cmd, mode);
#endif
}
inline int pclose(FILE* stream) {
#ifdef _WIN32
    return ::_pclose(stream);
#else
    return ::pclose(stream);
#endif
}

/** 从 popen/pclose 的原始返回值中提取子进程退出码。
 *  消除各调用点 #ifdef _WIN32 的分支代码。 */
inline int extract_exit_code(int pclose_status) {
#ifdef _WIN32
    return (pclose_status >= 0) ? ((pclose_status >> 8) & 0xFF) : -1;
#else
    return WIFEXITED(pclose_status) ? WEXITSTATUS(pclose_status) : -1;
#endif
}

/** 从 std::system() 的原始返回值中提取子进程退出码。
 *  Windows: system() 直接返回 cmd.exe 退出码
 *  Linux:   system() 返回 wait status，需用宏解码 */
inline int extract_system_code(int raw) {
#ifdef _WIN32
    return raw;
#else
    if (raw == -1) return -1;
    return WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
}

// ═══════════════════════════════════════════════════════════
// 时间与环境的跨平台适配
// ═══════════════════════════════════════════════════════════

/** 将 time_t 转换为本地时间 tm 结构（线程安全版本）。
 *  Windows: localtime_s, Linux/macOS: localtime_r */
inline void local_time(const std::time_t* t, std::tm* out) {
#ifdef _WIN32
    localtime_s(out, t);
#else
    localtime_r(t, out);
#endif
}

/** 设置环境变量（跨平台）。
 *  Windows: _putenv_s, Linux/macOS: setenv */
inline void set_env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    ::setenv(name, value, 1);
#endif
}

// ═══════════════════════════════════════════════════════════
// CPU 架构规范化
// ═══════════════════════════════════════════════════════════

/** 将 uname -m 结果或架构别名规范化到项目内部使用的架构标识。
 *  覆盖 uname -m 原生输出 + 常用别名，统一收口项目中散落的多处映射。 */
inline const char* normalize_arch(std::string_view machine) {
    if (machine.empty()) return "unknown";
    // ── x86 系列 ──
    if (machine == "x86_64" || machine == "amd64" || machine == "x64")
        return "amd64";
    if (machine == "i686" || machine == "i386" || machine == "x86" || machine == "x32")
        return "i386";
    // ── ARM 系列 ──
    if (machine == "aarch64" || machine == "arm64" || machine == "armv8l")
        return "arm64";
    if (machine == "armv7l" || machine == "armhf")
        return "armhf";
    if (machine == "armel")
        return "armel";
    // armv* 前缀匹配（armv6l, armv5l 等）
    if (machine.size() >= 4 && machine[0] == 'a' && machine[1] == 'r' &&
        machine[2] == 'm' && machine[3] == 'v')
        return (machine.size() > 4 && machine[4] == '5') ? "armel" : "armhf";
    // ── 其他架构 ──
    if (machine == "ppc64le" || machine == "ppc64el")
        return "ppc64el";
    if (machine == "s390x")
        return "s390x";
    return "unknown";
}

} // namespace tmoe::platform
#endif //PLATFORM_H
