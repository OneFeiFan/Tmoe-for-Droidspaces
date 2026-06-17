#pragma once
#include <string>
#include <memory>

#include "core/config.h"
#include "domain/runtime.h"

namespace tmoe::domain {
    enum class ContainerMode { Proot, Chroot, Nspawn };

    /// 容器领域实体 (Entity)
    class Container {
    public:
        Container(std::string name, std::string distro, std::string version,
                  std::string rootfs_path, ContainerMode mode, const TmoeConfig &cfg);

        const TmoeConfig &get_cfg() const { return cfg_; }

        // ── 自身行为 ──
        bool start(const LaunchContext* ctx = nullptr) const;

        bool stop() const;

        bool install() const;

        // ── 属性暴露 ──
        const std::string &name() const { return name_; }
        const std::string &distro() const { return distro_; }
        const std::string &version() const { return version_; }
        const std::string &rootfs_path() const { return rootfs_path_; }
        ContainerMode mode() const { return mode_; }

    private:
        std::string name_;
        std::string distro_;
        std::string version_;
        std::string rootfs_path_;
        ContainerMode mode_;
        const TmoeConfig &cfg_;

        // 聚合运行时策略，实现多态调用
        std::unique_ptr<IContainerRuntime> runtime_;
    };
} // namespace tmoe::domain
