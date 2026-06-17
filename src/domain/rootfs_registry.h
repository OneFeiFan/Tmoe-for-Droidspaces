//
// Created by 30225 on 2026/5/26.
//

#ifndef ROOTFS_REGISTRY_H
#define ROOTFS_REGISTRY_H
#pragma once
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp> // 假设你使用了现代 C++ 最流行的 JSON 库

namespace tmoe::domain {

    struct RootfsInfo {
        std::string distro;
        std::string version;
        std::string arch;
        std::string download_url; // 拼接好的完整下载地址
    };

    class RootfsRegistry {
    public:
        // 采用 Meyers' Singleton 模式，线程安全且无内存泄漏风险
        static RootfsRegistry& get_instance();

        // 禁用拷贝和赋值，符合单例规范
        RootfsRegistry(const RootfsRegistry&) = delete;
        RootfsRegistry& operator=(const RootfsRegistry&) = delete;

        // 动态设置当前镜像源（用于后续的 TUI 设置菜单）
        void set_mirror(const std::string& mirror_name);
        std::string get_current_mirror() const;

        // 获取支持的镜像源列表
        std::vector<std::string> get_available_mirrors() const;

        // 核心查询接口
        std::optional<RootfsInfo> query_rootfs(
            const std::string& distro,
            const std::string& version,
            const std::string& arch) const;

    private:
        RootfsRegistry();

        // 简单的字符串替换工具
        std::string replace_all(std::string str, const std::string& from, const std::string& to) const;

        nlohmann::json data_;
        std::string current_mirror_name_;
    };

} // namespace tmoe::domain
#endif //ROOTFS_REGISTRY_H
