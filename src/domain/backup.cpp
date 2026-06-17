#include "domain/backup.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {
    bool BackupManager::backup(std::string_view name, std::string_view format) {
        Logger::step("备份容器: " + std::string(name));
        // TODO: tar -cJf => backup_dir/{name}_{timestamp}.{format}
        return true;
    }

    bool BackupManager::restore(std::string_view archive_path) {
        Logger::step("恢复容器: " + std::string(archive_path));
        // TODO: tar -xf => container_root
        return true;
    }

    std::vector<std::string> BackupManager::list_backups() const {
        // TODO: 列出 backup_dir 下的备份文件
        return {};
    }
} // namespace tmoe::domain
