#include "domain/container.h"
#include "core/i18n.h"
#include "domain/runtime.h"

namespace tmoe::domain {
    Container::Container(std::string name, std::string distro, std::string version,
                         std::string rootfs_path, ContainerMode mode, const TmoeConfig &cfg)
        : name_(std::move(name)),
          distro_(std::move(distro)),
          version_(std::move(version)),
          rootfs_path_(std::move(rootfs_path)),
          mode_(mode),
          cfg_(cfg) {
        // 根据模式注入运行时策略
        switch (mode_) {
            case ContainerMode::Proot:
                runtime_ = std::make_unique<ProotRuntime>();
                break;
            case ContainerMode::Chroot: {
                ChrootRuntime::ChrootConfig cfg_;
                cfg_.rootfs_path = rootfs_path_;
                // 从配置文件加载（如有）
                runtime_ = std::make_unique<ChrootRuntime>(cfg_);
                break;
            }
            case ContainerMode::Nspawn: {
                NspawnRuntime::NspawnConfig cfg_;
                cfg_.rootfs_path = rootfs_path_;
                runtime_ = std::make_unique<NspawnRuntime>(cfg_);
                break;
            }
            default:
                throw std::invalid_argument(_("container.unsupported_mode"));
        }
    }

    bool Container::start(const LaunchContext *ctx) const {
        if (!runtime_) return false;
        return runtime_->start(*this, ctx);
    }

    bool Container::stop() const {
        if (!runtime_) return false;
        return runtime_->stop(*this);
    }

    bool Container::install() const {
        // 使用压缩包策略（避免需要原生 root 权限）
        RootfsTarballInstaller installer;
        return installer.install(*this);
    }
} // namespace tmoe::domain
