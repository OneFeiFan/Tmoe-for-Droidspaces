//
// Created by 30225 on 2026/5/25.
//
#include "domain/container_manager.h"
#include "core/logger.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tmoe::domain {
    // ── 容器工厂：实例化 ──
    Container ContainerManager::create(std::string_view name, std::string_view distro,
                                       std::string_view version, ContainerMode mode) const {
        fs::path rootfs_path = cfg_.container_root / name;
        fs::path config_file = rootfs_path / ".tmoe_env";

        // 1. 持久化保存 (如果是新建，或者是更新状态)
        if (!distro.empty() && distro != "unknown") {
            fs::create_directories(rootfs_path); // 确保目录存在
            std::ofstream ofs(config_file);
            if (ofs.is_open()) {
                ofs << distro << "\n" << version << "\n" << static_cast<int>(mode) << "\n";
            }
        }

        // 将 cfg_ 透传给容器实体
        return Container(std::string(name), std::string(distro), std::string(version),
                         rootfs_path.string(), mode, cfg_);
    }

    // ── 扫描本地已安装容器 ──
    std::vector<Container> ContainerManager::list_all() const {
        std::vector<Container> containers;
        if (!fs::exists(cfg_.container_root)) return containers;

        for (const auto &entry: fs::directory_iterator(cfg_.container_root)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                fs::path config_file = entry.path() / ".tmoe_env";

                std::string distro = "unknown", version = "unknown";
                ContainerMode mode = ContainerMode::Proot;

                // 2. 状态读取：真正的闭环，消除 unknown
                if (fs::exists(config_file)) {
                    std::ifstream ifs(config_file);
                    std::string mode_str;
                    if (std::getline(ifs, distro) && std::getline(ifs, version) && std::getline(ifs, mode_str)) {
                        mode = static_cast<ContainerMode>(std::stoi(mode_str));
                    }
                }
                containers.push_back(create(name, distro, version, mode));
            }
        }
        return containers;
    }

    // ── 精准查找 ──
    std::optional<Container> ContainerManager::find(std::string_view name) const {
        fs::path target = cfg_.container_root / name;
        if (fs::exists(target) && fs::is_directory(target)) {
            return create(name, "unknown", "unknown", ContainerMode::Proot);
        }
        return std::nullopt;
    }

    bool ContainerManager::remove(std::string_view name) const {
        fs::path target = cfg_.container_root / name;
        if (fs::exists(target)) {
            Logger::step("正在销毁容器数据: " + std::string(name));
            // 这里可以直接调用 fs::remove_all，也可以组装 rm -rf 给 Executor
            std::error_code ec;
            fs::remove_all(target, ec);
            return !ec;
        }
        return false;
    }

    std::vector<std::string> ContainerManager::available_distros() const {
        return {"debian", "ubuntu", "arch", "fedora", "alpine", "kali"};
    }
} // namespace tmoe::domain
