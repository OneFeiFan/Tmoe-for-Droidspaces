#include "domain/runtime/rootfs_registry.h"
#include "core/str_utils.h"

namespace tmoe::domain {
    RootfsRegistry &RootfsRegistry::get_instance() {
        static RootfsRegistry instance;
        return instance;
    }

    RootfsRegistry::RootfsRegistry() {
        try {
            data_ = nlohmann::json::parse(gen::ROOTFS_MAP_JSON);
            current_mirror_name_ = data_.value("default_mirror", "tuna");
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error(std::string("Failed to parse rootfs JSON: ") + e.what());
        }
    }

    void RootfsRegistry::set_mirror(const std::string &mirror_name) {
        if (data_["mirrors"].contains(mirror_name)) {
            current_mirror_name_ = mirror_name;
        } else {
            throw std::invalid_argument("未知的镜像源: " + mirror_name);
        }
    }

    std::string RootfsRegistry::get_current_mirror() const {
        return current_mirror_name_;
    }

    std::vector<std::string> RootfsRegistry::get_available_mirrors() const {
        std::vector<std::string> mirrors;
        for (auto &el: data_["mirrors"].items()) {
            mirrors.push_back(el.key());
        }
        return mirrors;
    }

    std::optional<RootfsInfo> RootfsRegistry::query_rootfs(
            const std::string &distro,
            const std::string &version,
            const std::string &arch) const {
        if (!data_["distributions"].contains(distro)) {
            return std::nullopt;
        }

        const auto &distro_data = data_["distributions"][distro];

        // 验证版本号
        auto versions = distro_data["supported_versions"].get<std::vector<std::string> >();
        if (std::find(versions.begin(), versions.end(), version) == versions.end()) {
            return std::nullopt;
        }

        // 验证架构
        auto archs = distro_data["supported_archs"].get<std::vector<std::string> >();
        if (std::find(archs.begin(), archs.end(), arch) == archs.end()) {
            return std::nullopt;
        }

        // 从镜像基地址 + 路径模板构建 URL
        std::string base_url = data_["mirrors"][current_mirror_name_];
        std::string path = distro_data["path_template"];
        path = replace_all(path, "{version}", version);
        path = replace_all(path, "{arch}", arch);

        RootfsInfo info;
        info.distro = distro;
        info.version = version;
        info.arch = arch;
        info.download_url = base_url + path;

        return info;
    }

} // namespace tmoe::domain
