//
// Created by 30225 on 2026/5/25.
//

#ifndef CONTAINER_MANAGER_H
#define CONTAINER_MANAGER_H
#pragma once
#include "core/config.h"
#include "domain/container.h"
#include <vector>
#include <optional>

namespace tmoe::domain {
    class ContainerManager {
    public:
        explicit ContainerManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /// 扫描根目录，返回所有安装的容器实例
        std::vector<Container> list_all() const;

        /// 精准查找
        std::optional<Container> find(std::string_view name) const;

        /// 容器工厂：实例化一个全新容器环境
        Container create(std::string_view name, std::string_view distro,
                         std::string_view version, ContainerMode mode = ContainerMode::Proot) const;

        /// 删除（从物理机移除）
        bool remove(std::string_view name) const;

        std::vector<std::string> available_distros() const;

    private:
        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
#endif //CONTAINER_MANAGER_H
