#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {
    /// Termux 专有功能管理
    /// 对应 Bash: share/termux/termux, share/termux/menu,
    ///           share/termux/backup, share/termux/mirror
    class TermuxManager {
    public:
        explicit TermuxManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /// 安装 Termux:X11 / Termux:GUI 支持
        bool install_x11_support();

        /// 备份 Termux home/data
        bool backup_termux();

        /// 恢复 Termux
        bool restore_termux(std::string_view archive_path);

        /// 美化终端（oh-my-zsh / colorls 等）
        bool beautify_terminal();

        /// 切换 Termux 软件源
        bool switch_pkg_mirror(std::string_view mirror);

        /// 首次运行的环境前置检查 (配色、字体、DNS)
        bool check_and_init_environment();

        /// 修复 Android 12+ 幽灵进程杀手导致的 Signal 9 崩溃
        bool fix_android_12_signal_9();

        /// 检查 Termux TUI 环境，并应用 libpopt 兼容性补丁
        /// @return 最终应当使用的 tui 二进制命令（如 "whiptail" 或包装器路径）
        std::string check_and_patch_tui_env();

    private:
        const TmoeConfig &cfg_;
        void termux_color_scheme_menu();
        void termux_font_menu();
        void configure_extra_keys();
    };
} // namespace tmoe::domain
