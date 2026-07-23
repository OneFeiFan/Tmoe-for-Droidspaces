#include "domain/runtime/container_manager.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "core/str_utils.h"

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
        if (!fs::exists(target)) return false;

        std::string name_str(name);
        std::string rootfs = target.string();

        // ── 安全检查（对应 Bash: remove_gnu_linux_container L40-L53）──
        // 1. 检查 /proc 是否非空（容器可能正在运行）
        fs::path proc_dir = target / "proc";
        if (fs::exists(proc_dir) && fs::is_directory(proc_dir)) {
            int entry_count = 0;
            for (const auto &entry: fs::directory_iterator(proc_dir)) {
                if (++entry_count > 2) break; // 超过 . 和 .. 即非空
            }
            if (entry_count > 2) {
                Logger::error(_("container.proc_not_empty"));
                Logger::info(_("container.stop_before_remove"));
                return false;
            }
        }

        // 2. 检查容器内是否有活跃挂载点（dev/shm, dev/pts, proc, sys）
        auto mount_check = Executor::shell(
                "grep -qs '" + rootfs + "/' /proc/mounts 2>/dev/null && echo 'mounted' || true");
        if (mount_check.ok() && !mount_check.stdout_data.empty()
            && mount_check.stdout_data.find("mounted") != std::string::npos) {
            Logger::error(_("container.has_active_mounts"));
            Logger::info(_("container.umount_before_remove"));
            return false;
        }

        // 3. 显示磁盘占用（Bash: du -sh --exclude=media/*）
        auto du_result = Executor::shell(
                "du -sh --exclude='" + rootfs + "/media/*' " + rootfs + " 2>/dev/null | awk '{print $1}' || true");
        if (du_result.ok() && !du_result.stdout_data.empty()) {
            std::string size = du_result.stdout_data;
            trim_newline(size);
            Logger::info(_f("container.disk_usage", size));
        }

        // 4. 确认删除
        if (!Logger::confirm(_f("container.confirm_remove", name_str))) {
            Logger::info(_("container.remove_cancelled"));
            return false;
        }

        Logger::step(_f("container.destroying", name_str));
        std::error_code ec;
        fs::remove_all(target, ec);
        if (ec) {
            Logger::error(_f("container.remove_failed", name_str));
            return false;
        }
        Logger::ok(_f("container.removed_ok", name_str));

        // ── 后续清理（对应 Bash: remove_gnu_linux_container L92-L129）──

        // 5. 删除启动器脚本 (R75: $PREFIX/bin 下的容器入口脚本)
        const char *prefix_env = std::getenv("PREFIX");
        std::string bin_dir = prefix_env ? (std::string(prefix_env) + "/bin") : "/usr/local/bin";
        auto cleanup_launcher = [&](const std::string &launcher) {
            fs::path p = fs::path(bin_dir) / launcher;
            if (fs::exists(p)) {
                Executor::passthrough(
                        CommandBuilder("rm").add_flag("-fv").add_arg(p.string()).build_string());
            }
        };
        cleanup_launcher(name_str);              // 容器同名启动器
        cleanup_launcher(name_str + "-rm");      // 容器-rm 启动器
        cleanup_launcher("startvnc");            // startvnc 符号链接
        cleanup_launcher("stopvnc");             // stopvnc 符号链接
        cleanup_launcher("startxsdl");           // startxsdl 符号链接

        // 6. 清理 /etc/profile 中的 alias (R76)
        Executor::shell(
                "sed -i '/alias " + name_str + "=/d' /etc/profile 2>/dev/null || true");
        Executor::shell(
                "sed -i '/alias " + name_str + "-rm=/d' /etc/profile 2>/dev/null || true");

        // 7. 提示是否删除 rootfs 镜像文件 (R78-R79)
        auto rootfs_files = Executor::shell(
                "ls " + cfg_.temp_dir.string() + "/" + name_str + "*-rootfs.tar.* 2>/dev/null || true");
        if (rootfs_files.ok() && !rootfs_files.stdout_data.empty()) {
            if (Logger::confirm(_("container.confirm_remove_rootfs"))) {
                Executor::passthrough(
                        CommandBuilder("rm").add_flag("-fv")
                                .add_arg(cfg_.temp_dir.string() + "/" + name_str + "*-rootfs.tar.*")
                                .build_string());
                Logger::ok(_("container.rootfs_removed"));
            }
        }

        return true;
    }

    std::vector<std::string> ContainerManager::available_distros() const {
        return {"debian", "ubuntu", "arch", "fedora", "alpine", "kali"};
    }
} // namespace tmoe::domain
