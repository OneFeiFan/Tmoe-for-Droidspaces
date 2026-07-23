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

        /** 启动 tmoe-zsh 管理工具 (外部 TUI 脚本)。 */
        bool start_tmoe_zsh();

        /** 将默认登录 shell 改为 zsh。 */
        bool change_shell_to_zsh();

        /** 配置 PulseAudio TCP 原生协议 (auth-ip-acl)。 */
        bool configure_pulseaudio_tcp();

        /** 设置 PulseAudio 空闲超时 (exit-idle-time)。 */
        bool configure_pulseaudio_idle_timeout();

        /** 切换局域网音频访问 (192.168.0.0/16;172.16.0.0/12)。 */
        bool toggle_lan_audio();

        // ═══════════════════════════════════════════════
        // 自更新
        // ═══════════════════════════════════════════════

        /** Git 拉取自更新 + fix-shebang。 */
        bool self_update();

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

        /** 旧版 Termux 镜像格式支持 (pre-2021)。 */
        void use_old_mirror_format(const std::string &mirror_url);

        /** 检测 Android 版本并限制旧版换源。 */
        bool check_android_version_for_mirror();

        /** 备份 sources.list + sources.list.d 为 tar.xz。 */
        void backup_sources_list();

        /** Alpine Linux 镜像切换。 */
        void switch_alpine_mirror();

        /** GNU/Linux 后备镜像处理。 */
        void linux_mirror_fallback();

        // ═══════════════════════════════════════════════
        // 磁盘空间
        // ═══════════════════════════════════════════════

        /** 查询 Termux 各目录磁盘占用 (TUI)。 */
        void check_disk_usage();

        /** 显示 Termux 目录大小排名。 */
        void show_termux_dir_usage();

        /** 显示 Termux 大文件 TOP30 列表。 */
        void show_termux_large_files();

        /** 显示 SD 卡空间占用。 */
        void show_sdcard_usage();

        /** 显示整体磁盘占用 (df -h)。 */
        void show_overall_disk_usage();

        // ═══════════════════════════════════════════════
        // 配色方案与字体
        // ═══════════════════════════════════════════════

        /** Termux 配色方案选择菜单。 */
        void termux_color_scheme_menu();

        /** Termux 字体选择菜单。 */
        void termux_font_menu();

        // ═══════════════════════════════════════════════
        // ADB / Signal 9 修复接口 (供 UI 插件调用)
        // ═══════════════════════════════════════════════

        /** ADB 连接并执行 Signal 9 修复。 */
        bool connect_adb_and_fix();

        /** 设置三星 ADB 兼容模式。 */
        bool set_samsung_adb_comp_mode();

        /** ADB 配对 + 连接双阶段流程。 */
        bool adb_pair_and_connect_flow();

        /** 可配置 ADB 服务器端口选择。 */
        bool select_adb_port();

        /** 基于 dumpsys 验证 Signal 9 修复结果。 */
        bool verify_signal9_fix();

        /** 执行 max_phantom_processes 修复命令。 */
        bool execute_max_phantom_fix(const std::string &adb_target);

        /** 统计已连接的 adb 设备数量。 */
        int count_adb_devices();

        // ═══════════════════════════════════════════════
        // 镜像源接口 (供 UI 插件调用)
        // ═══════════════════════════════════════════════

        /** 修改 Termux sources.list 指向指定镜像 URL。 */
        void modify_termux_sources_list(const std::string &mirror_url);

        // ═══════════════════════════════════════════════
        // 主菜单调度
        // ═══════════════════════════════════════════════

        // ── 以下方法提升为 public，供 UI 插件直接调用 ──

        /** 交互式选择桌面环境（xfce / lxqt）。 */
        std::string select_desktop_environment();

        /** 交互式选择 VNC 分辨率。 */
        std::string select_vnc_resolution();

        /** 手动选择备份文件（动态 whiptail 菜单）。 */
        std::string select_backup_file_manually();

        /** 获取备份文件名（inputbox）。 */
        std::string get_backup_filename();

    private:
        const TmoeConfig &cfg_;

        // 环境
        void configure_extra_keys();

        // 备份
        std::string select_backup_directories();

        void backup_termux_prepare(std::string &out_dir, std::string &out_filename, std::string &out_timestamp);

        // 还原
        std::string detect_latest_backup();

        bool uncompress_restore_archive(const std::string &archive_path);

        bool uncompress_zst(const std::string &file);

        bool uncompress_xz(const std::string &file);

        // 镜像源
        void write_termux_source(const std::string &file_path, const std::string &repo_name,
                                 const std::string &mirror_url, const std::string &component);

        void annotate_old_list(const std::string &file_path, const std::string &new_source_line);

        // Signal 9
        bool is_samsung_device();

        // X11/GUI
        void create_startvnc_script(const std::string &resolution, const std::string &desktop_env);

        /** 自动启动 RealVNC 查看器 (am start)。 */
        void auto_start_vnc_viewer();

        /** 自动启动文件管理器 (thunar/pcmanfm-qt)。 */
        void auto_start_file_manager_in_vnc();

        /** 检测并显示局域网 VNC IP 地址。 */
        std::string detect_and_show_lan_ip();

        /** 为桌面环境配置终端模拟器。 */
        std::string configure_terminal_emulator_for_de(const std::string &desktop_env);

        /** 使用 nano 手动编辑 startvnc 脚本。 */
        void edit_vnc_config_manually();

        /** 运行 termux-fix-shebang。 */
        void run_termux_fix_shebang(const std::string &file_path);

        /** GNU/Linux GUI 委托安装。 */
        void linux_gui_fallback();

        // Signal 9 增强
        /** 运行 ADB 命令并返回输出。 */
        std::string run_adb_cmd(const std::string &cmd);

        // 备份增强
        /** 调用 umount 脚本卸载挂载点。 */
        void unmount_before_backup();

        /** Timeshift 系统备份选项。 */
        void timeshift_backup_option();

        // OpenSSL 旧版修复
        /** 检测并安装 openssl-1.1 旧版兼容。 */
        void check_openssl_legacy();

        // 二进制到包的映射
        /** 将命令行工具名称映射到 Termux 包名。 */
        std::string map_binary_to_termux_pkg(const std::string &binary);

        // 备用 X11 会话
        /** 获取备用桌面会话 (如 XFCE → XFCE 4.18 回退)。 */
        std::string get_fallback_xsession(const std::string &desktop_env);
    };
} // namespace tmoe::domain
