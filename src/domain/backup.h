#pragma once
#include "core/config.h"
#include <string>
#include <vector>

namespace tmoe::domain {

/// 容器备份 / 恢复
/// 对应 Bash: share/compression/backup, share/compression/restore
class BackupManager {
public:
    explicit BackupManager(const TmoeConfig& cfg) : cfg_(cfg) {}

    /// 备份容器
    bool backup(std::string_view container_name,
                std::string_view format = "tar.xz");

    /// 恢复容器
    bool restore(std::string_view archive_path);

    /// 列出所有备份文件
    std::vector<std::string> list_backups() const;

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
