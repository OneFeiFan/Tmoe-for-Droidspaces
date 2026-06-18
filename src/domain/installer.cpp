#include "installer.hpp"
#include "core/i18n.h"

namespace tmoe::domain {
    bool RootfsTarballInstaller::install(const Container &container) {
        const auto &cfg = container.get_cfg();
        std::string rootfs_path = container.rootfs_path();
        Logger::step(_f("container.preparing_dir", rootfs_path));

        // 1. 确保目标目录存在
        Executor::shell("mkdir -p " + rootfs_path);

        // 2. 从数据驱动注册表查询 rootfs 下载地址
        auto rootfs_opt = RootfsRegistry::get_instance().query_rootfs(
            container.distro(), container.version(), cfg.arch
        );

        if (!rootfs_opt) {
            Logger::error(_("container.unsupported_combo"));
            return false;
        }

        std::string tarball_url = rootfs_opt->download_url;
        std::string tar_file = "/tmp/tmoe_rootfs.tar.xz";

        Logger::step(_("container.downloading_rootfs"));
        Logger::info(_f("container.download_url", tarball_url));

        // 3. 下载 rootfs 压缩包
        auto dl_cmd = CommandBuilder("curl")
                .add_arg("-L")
                .add_arg("-#")
                .add_arg("-o").add_arg(tar_file)
                .add_arg(tarball_url);

        if (!dl_cmd.execute().ok()) {
            Logger::error(_("container.download_failed"));
            return false;
        }

        // 4. 在 proot 沙箱中解压（Termux 下必须如此，否则会报 "operation not permitted"）
        Logger::step(_("container.extracting_rootfs"));
        auto extract_cmd = CommandBuilder("proot");

        extract_cmd.add_arg_if(cfg.is_termux, "--link2symlink")
                .add_arg("-0")
                .add_arg("tar")
                .add_arg("-xJf").add_arg(tar_file)
                .add_arg("-C").add_arg(rootfs_path)
                .add_arg("--exclude=dev/*")
                .add_arg("--exclude=sys/*")
                .add_arg("--exclude=proc/*");

        if (!extract_cmd.execute().ok()) {
            Logger::error(_("container.extract_failed"));
            return false;
        }

        Logger::ok(_f("container.install_complete", container.name(), container.distro()));
        Executor::shell("rm -f " + tar_file);
        return true;
    }
} // namespace tmoe::domain
