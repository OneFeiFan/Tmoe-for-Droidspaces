//
// Created by 30225 on 2026/5/25.
//

#include "installer.hpp"
#include "rootfs_registry.h" // 引入我们刚刚做的数据驱动注册表

#include "core/command_builder.hpp"
#include "core/logger.h"
#include "core/executor.h"

namespace tmoe::domain {

    bool RootfsTarballInstaller::install(const Container &container) {
        const auto &cfg = container.get_cfg();
        std::string rootfs_path = container.rootfs_path();
        Logger::step("正在准备容器目录: " + rootfs_path);

        // 1. 创建目录
        Executor::shell("mkdir -p " + rootfs_path);

        // ==========================================
        // 2. 获取 Rootfs (使用全新的 JSON 数据驱动)
        // ==========================================
        auto rootfs_opt = RootfsRegistry::get_instance().query_rootfs(
            container.distro(), container.version(), cfg.arch
        );

        if (!rootfs_opt) {
            Logger::error("获取镜像地址失败：不支持的 发行版/版本/架构 组合");
            return false;
        }

        std::string tarball_url = rootfs_opt->download_url;
        std::string tar_file = "/tmp/tmoe_rootfs.tar.xz";

        Logger::step("正在下载 Rootfs (请耐心等待)...");
        Logger::info("下载地址: " + tarball_url);

        // 使用你的 CommandBuilder 链式 API 执行下载
        auto dl_cmd = CommandBuilder("curl")
                .add_arg("-L")
                .add_arg("-#") // 增加进度条显示
                .add_arg("-o").add_arg(tar_file)
                .add_arg(tarball_url);

        if (!dl_cmd.execute().ok()) {
            Logger::error("Rootfs 下载失败!");
            return false;
        }

        // ==========================================
        // 3. 核心精髓：解压 (完美贴合原版 Termux 逻辑)
        // ==========================================
        Logger::step("正在解压 Rootfs 到指定容器路径...");
        auto extract_cmd = CommandBuilder("proot");

        // 原版神级修复：在 Termux 中解压必须强制 link2symlink 否则必报 operation not permitted
        extract_cmd.add_arg_if(cfg.is_termux, "--link2symlink")
                .add_arg("-0")
                .add_arg("tar")
                .add_arg("-xJf").add_arg(tar_file)
                .add_arg("-C").add_arg(rootfs_path)
                .add_arg("--exclude=dev/*")
                .add_arg("--exclude=sys/*")
                .add_arg("--exclude=proc/*");

        if (!extract_cmd.execute().ok()) {
            Logger::error("Rootfs 解压失败!");
            return false;
        }

        Logger::ok("容器 " + container.name() + " (" + container.distro() + ") 安装完毕！");
        Executor::shell("rm -f " + tar_file); // 清理缓存
        return true;
    }
} // namespace tmoe::domain