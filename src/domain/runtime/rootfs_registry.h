#ifndef ROOTFS_REGISTRY_H
#define ROOTFS_REGISTRY_H
#pragma once

#include "rootfs_data.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace tmoe::domain {
    /** rootfs 压缩包下载的元数据。 */
    struct RootfsInfo {
        std::string distro;
        std::string version;
        std::string arch;
        std::string download_url;
    };

    /** 数据驱动的 rootfs 注册表 (Meyers 单例)。
     *  读取编译期嵌入的 JSON 以解析下载 URL。
     */
    class RootfsRegistry {
    public:
        static RootfsRegistry &get_instance();

        RootfsRegistry(const RootfsRegistry &) = delete;

        RootfsRegistry &operator=(const RootfsRegistry &) = delete;

        /** 设置当前活动镜像源。 */
        void set_mirror(const std::string &mirror_name);

        /** 获取当前镜像源名称。 */
        std::string get_current_mirror() const;

        /** 列出所有可用镜像源名称。 */
        std::vector<std::string> get_available_mirrors() const;

        /** 查询指定发行版/版本/架构组合的下载 URL。 */
        std::optional<RootfsInfo> query_rootfs(
                const std::string &distro,
                const std::string &version,
                const std::string &arch) const;

    private:
        RootfsRegistry();


        nlohmann::json data_;
        std::string current_mirror_name_;
    };
} // namespace tmoe::domain
#endif //ROOTFS_REGISTRY_H
