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
        for (char c: s) {
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
    inline int yesno(const TmoeConfig &cfg,
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

/** 显示 whiptail --inputbox 对话框（文本输入）。
 *
 *  @param cfg       全局配置
 *  @param title     对话框标题（空则省略）
 *  @param text      提示文本
 *  @param init_text 初始文本（空则省略）
 *  @param height    对话框高度（0 = 自动）
 *  @param width     对话框宽度（0 = 自动）
 *  @return 用户输入的文本，取消返回空字符串
 */
    inline std::string inputbox(const TmoeConfig &cfg,
                                std::string_view title,
                                std::string_view text,
                                std::string_view init_text = "",
                                int height = 0, int width = 0) {
        std::string cmd = cfg.tui_bin;
        if (!title.empty()) {
            cmd += " --title \"";
            cmd += esc_dq(title);
            cmd += "\"";
        }
        cmd += " --inputbox \"";
        cmd += esc_dq(text);
        cmd += "\" ";
        cmd += std::to_string(height);
        cmd += " ";
        cmd += std::to_string(width);
        if (!init_text.empty()) {
            cmd += " \"";
            cmd += esc_dq(init_text);
            cmd += "\"";
        }

        bool cancelled = false;
        std::string result = Executor::tui_select(cmd, cancelled);
        return cancelled ? std::string{} : result;
    }

/** 显示 whiptail --fselect 对话框（文件选择器）。
 *
 *  @param cfg      全局配置
 *  @param title    对话框标题（空则省略）
 *  @param base_dir 起始目录（默认 /）
 *  @param height   对话框高度（0 = 自动）
 *  @param width    对话框宽度（0 = 自动）
 *  @return 用户选择的文件路径，取消返回空字符串
 */
    inline std::string fselect(const TmoeConfig &cfg,
                               std::string_view title,
                               std::string_view base_dir = "/",
                               int height = 0, int width = 0) {
        std::string cmd = cfg.tui_bin;
        if (!title.empty()) {
            cmd += " --title \"";
            cmd += esc_dq(title);
            cmd += "\"";
        }
        cmd += " --fselect \"";
        cmd += esc_dq(base_dir);
        cmd += "/\" ";
        cmd += std::to_string(height);
        cmd += " ";
        cmd += std::to_string(width);

        bool cancelled = false;
        std::string result = Executor::tui_select(cmd, cancelled);
        return cancelled ? std::string{} : result;
    }

/** 显示 whiptail --passwordbox 对话框（密码输入，输入内容被遮蔽）。
 *
 *  @param cfg    全局配置
 *  @param title  对话框标题（空则省略）
 *  @param text   提示文本
 *  @param height 对话框高度（0 = 自动）
 *  @param width  对话框宽度（0 = 自动）
 *  @return 用户输入的密码，取消返回空字符串
 */
    inline std::string passwordbox(const TmoeConfig &cfg,
                                   std::string_view title,
                                   std::string_view text,
                                   int height = 0, int width = 0) {
        std::string cmd = cfg.tui_bin;
        if (!title.empty()) {
            cmd += " --title \"";
            cmd += esc_dq(title);
            cmd += "\"";
        }
        cmd += " --passwordbox \"";
        cmd += esc_dq(text);
        cmd += "\" ";
        cmd += std::to_string(height);
        cmd += " ";
        cmd += std::to_string(width);

        bool cancelled = false;
        std::string result = Executor::tui_select(cmd, cancelled);
        return cancelled ? std::string{} : result;
    }

} // namespace tmoe::ui::dialog
