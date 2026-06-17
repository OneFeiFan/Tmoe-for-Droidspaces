#pragma once
#include <string>
#include <string_view>
#include <chrono>
#include <iostream>

namespace tmoe {

/// 彩色终端日志 + 进度条
class Logger {
public:
    // ── 颜色控制 ──
    static bool enable_color;
    static bool quiet_mode;

    enum Level { DEBUG, INFO, OK, WARN, ERROR };

    /// 基础日志
    static void log(Level lv, std::string_view msg);

    /// INFO 级别快捷方法
    static void info(std::string_view msg)  { log(INFO, msg); }
    static void ok(std::string_view msg)    { log(OK, msg); }
    static void warn(std::string_view msg)  { log(WARN, msg); }
    static void error(std::string_view msg) { log(ERROR, msg); }
    static void debug(std::string_view msg) { log(DEBUG, msg); }

    /// 根据 ExecResult 是否成功，输出 ok 或 error
    static void ok_or_fail(bool ok, std::string_view task);

    /// 获取当前时间戳（格式 YYYYMMDD_HHMMSS）
    static std::string timestamp();

    // ── 进度条 ──
    /// 显示进度条 (0.0 — 1.0)
    static void progress(double pct, std::string_view label = "");

    /// 显示步骤信息 " => 正在做某事..."
    static void step(std::string_view msg);

    // ── 用户交互 ──
    /// 等待用户按回车继续
    static void press_enter();

    /// 询问 y/n
    static bool confirm(std::string_view question);

private:
    static const char* ansi(Level lv);
};

} // namespace tmoe
