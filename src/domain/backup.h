#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <string>
#include <vector>

namespace tmoe::domain {

/** 容器备份/恢复。
 *  对应 Bash 脚本: share/compression/backup, share/compression/restore
 */
class BackupManager {
public:
    explicit BackupManager(const TmoeConfig& cfg) : cfg_(cfg) {}

    /** 将容器备份为归档文件。 */
    bool backup(std::string_view container_name,
                std::string_view format = "tar.xz");

    /** 从归档文件恢复容器。 */
    bool restore(std::string_view archive_path);

    /** 列出所有可用备份归档。 */
    std::vector<std::string> list_backups() const;

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
