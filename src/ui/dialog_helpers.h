/** whiptail 对话框工具函数。
 *  消除领域层中裸拼 cfg_.tui_bin + " --yesno ..." 字符串的模式。
 *  所有 whiptail 交互通过此文件中的函数完成。
 */
#pragma once
#include "core/config.h"
#include "core/executor.h"
#include <string>

namespace tmoe::ui::dialog {

/** 转义字符串中的双引号和反斜杠，用于嵌入 shell 双引号参数。 */
inline std::string esc_dq(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') r += '\\';
        r += c;
    }
    return r;
}

/** 显示 whiptail --yesno 对话框（双按钮选择）。
 *
 *  @param cfg      全局配置（提供 tui_bin 路径）
 *  @param title    对话框标题（空则省略 --title）
 *  @param text     提示文本
 *  @param yes_btn  Yes 按钮标签（空则用 whiptail 默认 "Yes"）
 *  @param no_btn   No 按钮标签（空则用 whiptail 默认 "No"）
 *  @param height   对话框高度（0 = whiptail 自动）
 *  @param width    对话框宽度（0 = whiptail 自动）
 *  @return 0=yes, 1=no, 255=ESC, -1=whiptail 未找到
 */
inline int yesno(const TmoeConfig& cfg,
                 std::string_view title,
                 std::string_view text,
                 std::string_view yes_btn = "",
                 std::string_view no_btn = "",
                 int height = 0, int width = 0) {
    std::string cmd = cfg.tui_bin;
    if (!title.empty()) {
        cmd += " --title \"";
        cmd += esc_dq(title);
        cmd += "\"";
    }
    if (!yes_btn.empty()) {
        cmd += " --yes-button \"";
        cmd += esc_dq(yes_btn);
        cmd += "\"";
    }
    if (!no_btn.empty()) {
        cmd += " --no-button \"";
        cmd += esc_dq(no_btn);
        cmd += "\"";
    }
    cmd += " --yesno \"";
    cmd += esc_dq(text);
    cmd += "\" ";
    cmd += std::to_string(height);
    cmd += " ";
    cmd += std::to_string(width);

    auto r = Executor::passthrough(cmd);
    return r.exit_code;
}

} // namespace tmoe::ui::dialog
