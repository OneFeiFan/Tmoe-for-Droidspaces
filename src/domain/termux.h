#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {
    /** Termux 专属特性管理。
     *  对应 Bash: share/termux/termux, share/termux/menu,
     *             share/termux/backup, share/termux/mirror,
     *             share/termux/restore, share/termux/fix_signal9,
     *             share/termux/xfce, share/termux/space_occupation
     */
    class TermuxManager {
    public:
        explicit TermuxManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        // ═══════════════════════════════════════════════
        // 环境初始化
        // ═══════════════════════════════════════════════

        /** 飞行前环境检查 (配色方案、字体、拓展按键)。 */
        bool check_and_init_environment();

        /** 修复 Android 12+ 幽灵进程杀手 (Signal 9) 问题。
         *  支持自动 ADB 连接 + 三星设备兼容。 */
        bool fix_android_12_signal_9();

        /** 检查 Termux TUI 环境并应用 libpopt 兼容补丁。
         *  @return 最终使用的 tui 二进制文件路径。 */
        std::string check_and_patch_tui_env();

        /** 设置 Termux 共享存储 (termux-setup-storage)。 */
        bool setup_storage();

        // ═══════════════════════════════════════════════
        // X11 / GUI 支持
        // ═══════════════════════════════════════════════

        /** 安装 Termux:X11 支持 (x11-repo + TigerVNC)。 */
        bool install_x11_support();

        /** 安装 Termux 原生 XFCE4 桌面环境。 */
        bool install_termux_xfce();

        /** 安装 Termux 原生 LXQt 桌面环境。 */
        bool install_termux_lxqt();

        /** 修改 Termux startvnc 的 VNC 分辨率等配置。 */
        bool configure_termux_vnc();

        /** 移除 Termux 已安装的 GUI 包 (xfce/lxqt/x11-repo)。 */
        bool remove_termux_gui();

        /** Termux 原生 GUI 管理子菜单。 */
        int run_termux_gui_menu();

        // ═══════════════════════════════════════════════
        // 备份与还原
        // ═══════════════════════════════════════════════

        /** 备份 Termux 家目录/usr 到 SD 卡 (tar + zstd/xz)。
         *  @return 备份是否成功。 */
        bool backup_termux();

        /** 从备份归档中还原 Termux。
         *  @param archive_path 若为空则自动检测最新备份。 */
        bool restore_termux(std::string_view archive_path = "");

        // ═══════════════════════════════════════════════
        // 终端美化
        // ═══════════════════════════════════════════════

        /** 美化终端 (oh-my-zsh / tmoe-zsh 工具箱)。 */
        bool beautify_terminal();

        /** 配置 tmoe-zsh (zinit + 主题 + 插件)。 */
        bool configure_tmoe_zsh();

        /** 将默认登录 shell 改为 zsh。 */
        bool change_shell_to_zsh();

        // ═══════════════════════════════════════════════
        // 软件包镜像源
        // ═══════════════════════════════════════════════

        /** 切换 Termux 软件包镜像源（交互式 TUI 菜单）。 */
        bool switch_pkg_mirror(std::string_view mirror = "");

        /** 启用/禁用 Termux 额外仓库 (game/root/science/unstable/x11)。 */
        void manage_termux_repos();

        /** 镜像站下载速度测试 (aria2c 对比 clang .deb)。 */
        void run_mirror_speed_test();

        /** 手动编辑 sources.list。 */
        void edit_sources_manually();

        /** 清理 sources.list 无效/重复行。 */
        void clean_sources_list();

        /** 恢复 Termux 默认官方源。 */
        void restore_default_sources();

        // ═══════════════════════════════════════════════
        // 磁盘空间
        // ═══════════════════════════════════════════════

        /** 查询 Termux 各目录磁盘占用 (TUI)。 */
        void check_disk_usage();

        // ═══════════════════════════════════════════════
        // 主菜单调度
        // ═══════════════════════════════════════════════

        /** Termux 专用功能总菜单 (TUI)。 */
        int run_termux_menu();

    private:
        const TmoeConfig &cfg_;

        // 环境
        void termux_color_scheme_menu();

        void termux_font_menu();

        void configure_extra_keys();

        // 备份
        std::string get_backup_filename();

        std::string select_backup_directories();

        void backup_termux_prepare(std::string &out_dir, std::string &out_filename, std::string &out_timestamp);

        // 还原
        std::string detect_latest_backup();

        std::string select_backup_file_manually();

        bool uncompress_restore_archive(const std::string &archive_path);

        bool uncompress_zst(const std::string &file);

        bool uncompress_xz(const std::string &file);

        // 镜像源
        void modify_termux_sources_list(const std::string &mirror_url);

        void write_termux_source(const std::string &file_path, const std::string &repo_name,
                                 const std::string &mirror_url, const std::string &component);

        void annotate_old_list(const std::string &file_path, const std::string &new_source_line);

        // Signal 9
        bool connect_adb_and_fix();

        bool is_samsung_device();

        bool execute_max_phantom_fix(const std::string &adb_target);

        // 磁盘
        void show_termux_dir_usage();

        void show_termux_large_files();

        void show_sdcard_usage();

        void show_overall_disk_usage();

        // X11/GUI
        void create_startvnc_script(const std::string &resolution, const std::string &desktop_env);

        std::string select_vnc_resolution();

        std::string select_desktop_environment();
    };
} // namespace tmoe::domain
