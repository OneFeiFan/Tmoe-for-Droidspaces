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

    /** 清理容器垃圾（缓存/日志/历史记录/apt）。 */
    bool clean_container_garbage(std::string_view rootfs_path);

    // ── 容器恢复 ──
    /** 从归档文件恢复容器（5 种模式控制器）。 */
    bool restore_container(std::string_view archive_path = "");

    /** 解压归档文件到目标路径。 */
    bool uncompress_archive(std::string_view archive_path,
                            std::string_view target_path);

    // ── 辅助 ──
    /** 列出备份目录中所有压缩包。 */
    std::vector<std::string> list_backups() const;

    /** 检测最新备份文件。 */
    std::string detect_latest_backup() const;

    /** 根据归档扩展名检测格式。 */
    static ArchiveFormat detect_format(std::string_view path);

    /** 生成备份文件名（含时间戳）。 */
    static std::string generate_filename(std::string_view container_name,
                                          std::string_view distro,
                                          std::string_view desktop = "");

    /** 获取备份文件大小 (MB)。 */
    static int64_t get_archive_size_mb(std::string_view path);

private:
    const TmoeConfig& cfg_;

    // ── 内部方法 ──
    /** 执行 tar 打包压缩。 */
    bool run_tar_backup(std::string_view source_dir,
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

    /** 获取垃圾文件列表并询问确认。 */
    bool tui_confirm_cleanup(std::string_view rootfs_path);
};

} // namespace tmoe::domain
