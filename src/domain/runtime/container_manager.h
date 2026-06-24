#ifndef CONTAINER_MANAGER_H
#define CONTAINER_MANAGER_H
#pragma once
#include "core/config.h"
#include "domain/runtime/container.h"
#include "core/logger.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <optional>

namespace tmoe::domain {
    /** 容器生命周期管理器 — 列出、查找、创建、移除。 */
    class ContainerManager {
    public:
        explicit ContainerManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /** 扫描容器根目录并返回所有已安装容器。 */
        [[nodiscard]] std::vector<Container> list_all() const;

        /** 按名称查找容器。 */
        [[nodiscard]] std::optional<Container> find(std::string_view name) const;

        /** 工厂方法: 创建新的容器实例。 */
        [[nodiscard]] Container create(std::string_view name, std::string_view distro,
                                       std::string_view version, ContainerMode mode = ContainerMode::Proot) const;

        /** 从磁盘物理移除容器。 */
        [[nodiscard]] bool remove(std::string_view name) const;

        /** 列出支持的发行版名称。 */
        [[nodiscard]] std::vector<std::string> available_distros() const;

    private:
        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
#endif //CONTAINER_MANAGER_H
