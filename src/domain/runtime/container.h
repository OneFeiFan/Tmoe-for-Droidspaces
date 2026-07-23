#pragma once

#include <string>
#include <memory>
#include "core/logger.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "core/config.h"
#include "domain/runtime/runtime.h"
#include "domain/runtime/installer.hpp"
#include <stdexcept>

namespace tmoe::domain {
    enum class ContainerMode {
        Proot, Chroot, Nspawn
    };

    /** 容器领域实体。
     *  聚合运行时策略以实现多态调用。
     */
    class Container {
    public:
        Container(std::string name, std::string distro, std::string version,
                  std::string rootfs_path, ContainerMode mode, const TmoeConfig &cfg);

        [[nodiscard]] const TmoeConfig &get_cfg() const { return cfg_; }

        // ── Lifecycle ──
        bool start(const LaunchContext *ctx = nullptr) const;

        [[nodiscard]] bool stop() const;

        [[nodiscard]] bool install() const;

        // ── Accessors ──
        [[nodiscard]] const std::string &name() const { return name_; }

        [[nodiscard]] const std::string &distro() const { return distro_; }

        [[nodiscard]] const std::string &version() const { return version_; }

        [[nodiscard]] const std::string &rootfs_path() const { return rootfs_path_; }

        [[nodiscard]] ContainerMode mode() const { return mode_; }

    private:
        std::string name_;
        std::string distro_;
        std::string version_;
        std::string rootfs_path_;
        ContainerMode mode_;
        const TmoeConfig &cfg_;

        /** 运行时策略（多态分发）。 */
        std::unique_ptr<IContainerRuntime> runtime_;
    };
} // namespace tmoe::domain
