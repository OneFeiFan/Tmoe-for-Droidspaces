#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {

/** 备份归档格式。 */
enum class ArchiveFormat { TarZst, TarXz, TarGz, Unknown };

/** 备份恢复操作，对应 Bash 脚本:
 *   share/compression/backup
 *   share/compression/restore
 *   share/termux/backup
 */
class BackupManager {
public:
    explicit BackupManager(const TmoeConfig& cfg);

    // ── TUI 入口 ──
    /** 备份/恢复主菜单 (whiptail TUI)。 */
    void run_backup_menu();

    // ── 容器备份 ──
    /** 执行容器备份（完整流程：清理垃圾→打包→压缩）。*/
    bool backup_container(std::string_view container_name,
                          std::string_view rootfs_path);

    /** 备份至外置 SD/TF 卡 (仅 Termux 环境)。 */
    bool backup_to_external_storage(std::string_view container_name,
                                    std::string_view rootfs_path);

    // ── 容器清理 ──
    /** 清理容器垃圾（多发行版：apt/pacman/dnf/apk）。 */
    bool clean_container_garbage(std::string_view rootfs_path);

    /** 按发行版清理包缓存。 */
    bool clean_package_cache(std::string_view rootfs_path);

    /** 清理用户级垃圾（VNC/ZSH/应用缓存/conda/cargo/go...）。 */
    bool clean_user_garbage(std::string_view rootfs_path);

    /** 清理 systemd journal 日志。 */
    bool clean_systemd_journal(std::string_view rootfs_path);

    /** 统计并展示垃圾清理前后的大小变化。 */
    void show_garbage_stats(std::string_view rootfs_path);

    // ── 容器恢复 ──
    /** 从归档文件恢复容器（5 种模式控制器）。 */
    bool restore_container(std::string_view archive_path = "");

    /** 解压归档文件到目标路径（支持通用 tar 回退）。 */
    bool uncompress_archive(std::string_view archive_path,
                            std::string_view target_path,
                            bool show_progress = false);

    // ── 桌面环境检测 ──
    /** 检测容器内安装的桌面环境。 */
    std::string detect_desktop_environment(std::string_view rootfs_path);

    /** 检测并设置发行版类型（自动设置 cfg_ 中的 linux_distro）。 */
    std::string detect_container_distro(std::string_view rootfs_path);

    // ── 辅助 ──
    /** 列出备份目录中所有压缩包。 */
    std::vector<std::string> list_backups() const;

    /** 列出备份目录中所有压缩包（含大小和时间）。 */
    std::vector<std::tuple<std::string, int64_t, std::string>> list_backups_detailed() const;

    /** 检测最新备份文件。 */
    std::string detect_latest_backup() const;

    /** 根据归档扩展名检测格式。 */
    static ArchiveFormat detect_format(std::string_view path);

    /** 生成备份文件名（含时间戳+DE名称）。 */
    std::string generate_filename(std::string_view container_name,
                                   std::string_view distro = "",
                                   std::string_view desktop = "");

    /** 获取备份文件大小 (MB)。 */
    static int64_t get_archive_size_mb(std::string_view path);

    /** 获取备份文件的人类可读大小。 */
    static std::string get_archive_size_human(std::string_view path);

    /** 收集额外备份目标（tmoe脚本/QEMU库/配置）。 */
    std::vector<std::string> collect_extra_backup_targets(std::string_view container_name);

    /** 运行 Timeshift 系统快照。 */
    bool run_timeshift_backup();

    /** 安装跨发行版 Timeshift。 */
    bool install_timeshift();

    /** 容器移除前备份提示。 */
    bool prompt_backup_before_removal(std::string_view container_name,
                                       std::string_view rootfs_path);

private:
    const TmoeConfig& cfg_;

    // ── 内部方法 ──
    /** 执行 tar 打包压缩。 */
    bool run_tar_backup(std::string_view source_dir,
                        std::string_view output_file,
                        ArchiveFormat format,
                        const std::vector<std::string>& excludes = {},
                        const std::vector<std::string>& includes = {});

    /** 使用 pv 显示进度的 tar 打包。 */
    bool run_tar_backup_with_progress(std::string_view source_dir,
                                       std::string_view output_file,
                                       ArchiveFormat format,
                                       const std::vector<std::string>& excludes = {});

    /** TUI 让用户选择备份格式。 */
    ArchiveFormat tui_select_format();

    /** TUI 让用户选择恢复模式。 */
    enum class RestoreMode { Normal, FromSD, ManualPath, FromDownload, Compat };
    RestoreMode tui_select_restore_mode();

    /** TUI 让用户手动选择备份文件。 */
    std::string tui_select_file(const std::string& base_dir);

    /** TUI 让用户多选备份目标。 */
    std::vector<std::string> tui_multi_select_targets();

    /** 获取垃圾文件列表并询问确认。 */
    bool tui_confirm_cleanup(std::string_view rootfs_path);

    /** 判断是否为 chroot 容器。 */
    bool is_chroot_container(std::string_view rootfs_path) const;

    /** 获取 tar 命令前缀（chroot 加 sudo）。 */
    std::string get_tar_prefix() const;

    /** 检测是否有 pv 可用。 */
    bool has_pv() const;

    /** 获取容器内可用的发行版包管理器。 */
    std::string detect_package_manager(std::string_view rootfs_path);
};

} // namespace tmoe::domain
