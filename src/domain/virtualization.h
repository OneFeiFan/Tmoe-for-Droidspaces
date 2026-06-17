#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

/// 虚拟化管理 (QEMU / Docker)
/// 对应 Bash: tools/virtualization/virt-menu,
///           tools/virtualization/qemu/*,
///           tools/virtualization/docker/*
class VirtualizationManager {
public:
    explicit VirtualizationManager(const TmoeConfig& cfg) : cfg_(cfg) {}

    // ── QEMU ──
    bool qemu_create(std::string_view name, int64_t disk_size_gb,
                     int ram_mb, int cpu_cores);
    bool qemu_start(std::string_view name);
    bool qemu_stop(std::string_view name);
    bool qemu_list() const;

    // ── Docker ──
    bool docker_install();
    bool docker_pull(std::string_view image);
    bool docker_run(std::string_view image, std::string_view name);

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
