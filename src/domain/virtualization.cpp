#include "domain/virtualization.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {
    // ── QEMU ──
    bool VirtualizationManager::qemu_create(std::string_view name,
                                            int64_t disk_size_gb,
                                            int ram_mb, int cpu_cores) {
        Logger::step("创建 QEMU 虚拟机: " + std::string(name));
        // TODO: qemu-img create + 生成启动脚本
        return true;
    }

    bool VirtualizationManager::qemu_start(std::string_view name) {
        Logger::step("启动虚拟机: " + std::string(name));
        return true;
    }

    bool VirtualizationManager::qemu_stop(std::string_view name) {
        Logger::step("停止虚拟机: " + std::string(name));
        return true;
    }

    bool VirtualizationManager::qemu_list() const {
        return true;
    }

    // ── Docker ──
    bool VirtualizationManager::docker_install() {
        Logger::step("安装 Docker");
        // TODO: 检测环境并调用对应安装脚本
        return true;
    }

    bool VirtualizationManager::docker_pull(std::string_view image) {
        Logger::step("拉取镜像: " + std::string(image));
        return true;
    }

    bool VirtualizationManager::docker_run(std::string_view image,
                                           std::string_view name) {
        Logger::step("运行容器: " + std::string(name));
        return true;
    }
} // namespace tmoe::domain
