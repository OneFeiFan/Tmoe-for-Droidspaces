#pragma once
#include <string>
#include <string_view>
#include <chrono>
#include <iostream>

namespace tmoe {

/** 彩色终端日志 + 进度条支持。 */
class Logger {
public:
    static bool enable_color;
    static bool quiet_mode;

    enum Level { DEBUG, INFO, OK, WARN, ERROR };

    /** 按指定级别输出日志消息。 */
    static void log(Level lv, std::string_view msg);

    /// @name 快捷辅助方法
    /// @{
    static void info(std::string_view msg)  { log(INFO, msg); }
    static void ok(std::string_view msg)    { log(OK, msg); }
    static void warn(std::string_view msg)  { log(WARN, msg); }
    static void error(std::string_view msg) { log(ERROR, msg); }
    static void debug(std::string_view msg) { log(DEBUG, msg); }
    /// @}

    /** 根据布尔结果输出 [✓] 或 [✗]。 */
    static void ok_or_fail(bool ok, std::string_view task);

    /** 获取当前时间戳 (YYYYMMDD_HHMMSS 格式)。 */
    static std::string timestamp();

    /** 绘制进度条 (0.0 – 1.0)。 */
    static void progress(double pct, std::string_view label = "");

    /** 打印步骤指示符 " => ..."。 */
    static void step(std::string_view msg);

    /** 等待用户按回车继续。 */
    static void press_enter();

    /** 询问 y/n 问题；返回 true 表示同意。 */
    static bool confirm(std::string_view question);

private:
    static const char* ansi(Level lv);
};

} // namespace tmoe
