//
// Created by 30225 on 2026/5/25.
//

#ifndef INSTALLER_HPP
#define INSTALLER_HPP
#pragma once
#include "domain/container.h"

namespace tmoe::domain {
    /// 容器安装器接口
    class IInstaller {
    public:
        virtual ~IInstaller() = default;

        virtual bool install(const Container &container) = 0;
    };

    /// 基于 Rootfs 压缩包下载解压的实用主义安装策略
    class RootfsTarballInstaller : public IInstaller {
    public:
        RootfsTarballInstaller() = default;

        bool install(const Container &container) override;

    };
} // namespace tmoe::domain
#endif //INSTALLER_HPP
