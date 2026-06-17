#pragma once
#include "core/config.h"
#include <fstream>
#include "core/executor.h"
#include "core/logger.h"
#include <string>

namespace tmoe::domain {
    /** Termux 专属特性管理。
     *  对应 Bash: share/termux/termux, share/termux/menu,
     *             share/termux/backup, share/termux/mirror
     */
    class TermuxManager {
    public:
        explicit TermuxManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /** 安装 Termux:X11 / Termux:GUI 支持。 */
        bool install_x11_support();

        /** 备份 Termux 家目录/数据。 */
        bool backup_termux();

        /** 从备份归档中还原 Termux。 */
        bool restore_termux(std::string_view archive_path);

        /** 美化终端 (oh-my-zsh / colorls / 字体)。 */
        bool beautify_terminal();

        /** 切换 Termux 软件包镜像源。 */
        bool switch_pkg_mirror(std::string_view mirror);

        /** 飞行前环境检查 (配色方案、字体、拓展按键)。 */
        bool check_and_init_environment();

        /** 修复 Android 12+ 幽灵进程杀手 (Signal 9) 问题。 */
        bool fix_android_12_signal_9();

        /** 检查 Termux TUI 环境并应用 libpopt 兼容补丁。
         *  @return 最终使用的 tui 二进制文件 (例如 "whiptail" 或包装脚本路径)。
         */
        std::string check_and_patch_tui_env();

    private:
        const TmoeConfig &cfg_;

        void termux_color_scheme_menu();

        void termux_font_menu();

        void configure_extra_keys();
    };
} // namespace tmoe::domain
