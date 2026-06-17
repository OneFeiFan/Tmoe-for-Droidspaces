#include "core/logger.h"
#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace tmoe {

bool Logger::enable_color = true;
bool Logger::quiet_mode    = false;

const char* Logger::ansi(Level lv) {
    if (!enable_color) return "";
    switch (lv) {
        case DEBUG: return "\033[90m";   // 灰
        case INFO:  return "\033[36m";   // 青
        case OK:    return "\033[32m";   // 绿
        case WARN:  return "\033[33m";   // 黄
        case ERROR: return "\033[31m";   // 红
        default:    return "\033[0m";
    }
}

static const char* RESET = "\033[0m";

void Logger::log(Level lv, std::string_view msg) {
    if (quiet_mode && lv == DEBUG) return;

    const char* prefix = "";
    switch (lv) {
        case DEBUG: prefix = "[DEBUG] "; break;
        case INFO:  prefix = "[*] ";     break;
        case OK:    prefix = "[✓] ";     break;
        case WARN:  prefix = "[!] ";     break;
        case ERROR: prefix = "[✗] ";     break;
    }
    std::fprintf(stderr, "%s%s%s%s\n", ansi(lv), prefix, RESET, msg.data());
}

void Logger::ok_or_fail(bool ok, std::string_view task) {
    if (ok) {
        std::fprintf(stderr, "%s[✓] %s%s\n", ansi(OK), RESET, task.data());
    } else {
        std::fprintf(stderr, "%s[✗] %s failed%s\n", ansi(ERROR), RESET, task.data());
    }
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto tm  = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

void Logger::progress(double pct, std::string_view label) {
    if (quiet_mode) return;
    int bar_width = 40;
    int filled = static_cast<int>(pct * bar_width);
    std::fprintf(stderr, "\r[");
    for (int i = 0; i < bar_width; ++i)
        std::fputc(i < filled ? '=' : ' ', stderr);
    std::fprintf(stderr, "] %3d%% %s", static_cast<int>(pct * 100), label.data());
    if (pct >= 1.0) std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

void Logger::step(std::string_view msg) {
    std::fprintf(stderr, "%s => %s%s\n", ansi(INFO), RESET, msg.data());
}

void Logger::press_enter() {
    if (quiet_mode) return;
    std::fprintf(stderr, "%s[ENTER] 按回车继续...%s", ansi(INFO), RESET);
    std::fflush(stderr);
    std::getchar();
}

bool Logger::confirm(std::string_view question) {
    std::fprintf(stderr, "%s[?] %s (y/N) %s", ansi(WARN), question.data(), RESET);
    std::fflush(stderr);
    int ch = std::getchar();
    return (ch == 'y' || ch == 'Y');
}

} // namespace tmoe
