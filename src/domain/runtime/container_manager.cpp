#include "domain/runtime/container_manager.h"
#include "core/i18n.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    Container ContainerManager::create(std::string_view name, std::string_view distro,
                                       std::string_view version, ContainerMode mode) const {
        fs::path rootfs_path = cfg_.container_root / name;
        fs::path config_file = rootfs_path / ".tmoe_env";

        // 持久化保存新建或更新容器的元数据
        if (!distro.empty() && distro != "unknown") {
            fs::create_directories(rootfs_path);
            std::ofstream ofs(config_file);
            if (ofs.is_open()) {
                ofs << distro << "\n" << version << "\n" << static_cast<int>(mode) << "\n";
            }
        }

        return Container(std::string(name), std::string(distro), std::string(version),
                         rootfs_path.string(), mode, cfg_);
    }

    std::vector<Container> ContainerManager::list_all() const {
        std::vector<Container> containers;
        if (!fs::exists(cfg_.container_root)) return containers;

        for (const auto &entry: fs::directory_iterator(cfg_.container_root)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                fs::path config_file = entry.path() / ".tmoe_env";

                std::string distro = "unknown", version = "unknown";
                ContainerMode mode = ContainerMode::Proot;

                // 从磁盘还原元数据
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
            Logger::step(_f("container.destroying", std::string(name)));
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
