#ifndef INSTALLER_HPP
#define INSTALLER_HPP
#pragma once
#include "domain/runtime/container.h"
#include "domain/runtime/rootfs_registry.h"
#include "core/command_builder.hpp"
#include "core/logger.h"
#include "core/executor.h"

namespace tmoe::domain {
    /** 容器安装器接口（策略模式）。 */
    class IInstaller {
    public:
        virtual ~IInstaller() = default;

        virtual bool install(const Container &container) = 0;
    };

    /** 基于 Rootfs 压缩包下载解压的安装策略。 */
    class RootfsTarballInstaller : public IInstaller {
    public:
        RootfsTarballInstaller() = default;

        bool install(const Container &container) override;
    };
} // namespace tmoe::domain
#endif //INSTALLER_HPP
