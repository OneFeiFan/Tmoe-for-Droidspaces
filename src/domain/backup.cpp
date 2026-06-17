#include "domain/backup.h"

namespace tmoe::domain {
    bool BackupManager::backup(std::string_view name, std::string_view format) {
        Logger::step("备份容器: " + std::string(name));
        // TODO: tar -cJf 到 backup_dir/{name}_{timestamp}.{format}
        return true;
    }

    bool BackupManager::restore(std::string_view archive_path) {
        Logger::step("恢复容器: " + std::string(archive_path));
        // TODO: tar -xf 到 container_root
        return true;
    }

    std::vector<std::string> BackupManager::list_backups() const {
        // TODO: 枚举 backup_dir 下的备份文件
        return {};
    }
} // namespace tmoe::domain
