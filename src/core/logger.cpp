#include "core/logger.h"
#include "core/i18n.h"
#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>
#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#endif

namespace tmoe {
    bool Logger::enable_color = true;
    bool Logger::quiet_mode = false;

    const char *Logger::ansi(Level lv) {
        if (!enable_color) return "";
        switch (lv) {
            case DEBUG: return "\033[90m"; // 灰
            case INFO: return "\033[36m"; // 青
            case OK: return "\033[32m"; // 绿
            case WARN: return "\033[33m"; // 黄
            case ERROR: return "\033[31m"; // 红
            default: return "\033[0m";
        }
    }

    static const char *RESET = "\033[0m";

    void Logger::log(Level lv, std::string_view msg) {
        if (quiet_mode && lv == DEBUG) return;

        const char *prefix = "";
        switch (lv) {
            case DEBUG: prefix = "[DEBUG] ";
                break;
            case INFO: prefix = "[*] ";
                break;
            case OK: prefix = "[✓] ";
                break;
            case WARN: prefix = "[!] ";
                break;
            case ERROR: prefix = "[✗] ";
                break;
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
        auto t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&t);
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
        std::fprintf(stderr, "%s[ENTER] %s%s", ansi(INFO), RESET, _("misc.press_enter").c_str());
        std::fflush(stderr);
#ifndef _WIN32
        // 排空 stdin 缓冲（tcflush 清终端驱动层，read 清 stdio 内部缓冲），
        // 避免残留字符导致 press_enter 连锁触发
        tcflush(STDIN_FILENO, TCIFLUSH);
        char c;
        while (::read(STDIN_FILENO, &c, 1) == 1 && c != '\n') {}
#else
        std::getchar();
#endif
    }

    bool Logger::confirm(std::string_view question) {
        std::fprintf(stderr, "%s[?] %s (y/N) %s", ansi(WARN), question.data(), RESET);
        std::fflush(stderr);

#ifndef _WIN32
        tcflush(STDIN_FILENO, TCIFLUSH);
        char dummy;
        while (::read(STDIN_FILENO, &dummy, 1) == 1 && dummy != '\n') {}
#endif

        int ch = std::getchar();
        // 消费该行剩余内容 (用户可能键入 "yes" + Enter)
        if (ch != EOF && ch != '\n') {
            int c;
            while ((c = std::getchar()) != '\n' && c != EOF) {
            }
        }
        return (ch == 'y' || ch == 'Y');
    }
} // namespace tmoe
