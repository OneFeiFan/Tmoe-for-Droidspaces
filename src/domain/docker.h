#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <string>
#include <vector>
#include <filesystem>

namespace tmoe::domain {

/** Docker 镜像信息。 */
struct DockerImageInfo {
    std::string name;       // 如 "debian"
    std::string tag;        // 如 "latest"
    std::string full_name() const { return name + ":" + tag; }
};

/** Docker 容器管理，对应 Bash 脚本:
 *   tools/virtualization/docker/docker_menu
 *   tools/virtualization/docker/configure
 */
class DockerManager {
public:
    explicit DockerManager(const TmoeConfig& cfg);

    // ── TUI 入口 ──
    /** Docker 管理主菜单 (whiptail TUI)。 */
    void run_docker_menu();

    // ── Docker 安装 ──
    /** 安装 Docker CE (社区版)。 */
    bool install_docker_ce();

    /** 安装 Docker.io (Debian 仓库版)。 */
    bool install_docker_io();

    /** 安装 Portainer Web 管理界面。 */
    bool install_portainer(int port = 9000);

    // ── 镜像管理 ──
    /** 拉取 Docker 镜像。 */
    bool pull_image(std::string_view image, std::string_view tag = "latest");

    /** 列出本地 Docker 镜像。 */
    std::vector<DockerImageInfo> list_images() const;

    /** 选择并拉取 Linux 发行版镜像 (TUI)。 */
    bool tui_pull_distro_image();

    // ── 容器管理 ──
    /** 运行 Docker 容器（特权模式 + 持久化挂载）。 */
    bool run_container(std::string_view image, std::string_view name,
                       std::string_view mount_path = "",
                       int host_port = 0, int container_port = 0);

    /** 以跨架构模式运行容器 (qemu-user-static)。 */
    bool run_cross_arch_container(std::string_view image,
                                  std::string_view name,
                                  std::string_view arch);

    /** 列出运行中的 Docker 容器。 */
    std::vector<std::string> list_containers(bool all = false) const;

    /** 停止并删除容器。 */
    bool remove_container(std::string_view name);

    // ── 导入/导出 ──
    /** 导出容器为 tar 文件。 */
    bool export_container(std::string_view name, std::string_view output_path);

    /** 从 tar 文件导入为 Docker 镜像。 */
    bool import_image(std::string_view tar_path,
                      std::string_view image_name,
                      std::string_view tag = "latest");

    // ── Docker 配置 ──
    /** 配置 Docker 镜像源 (163 等)。 */
    bool configure_mirror();

    /** 添加当前用户到 docker 组。 */
    bool add_user_to_docker_group();

    /** 安装 qemu-user-static 跨架构支持。 */
    bool install_qemu_user_static();

    /** 检查 Docker 是否已安装并可运行。 */
    bool is_docker_available() const;

private:
    const TmoeConfig& cfg_;

    // ── 内部辅助 ──
    std::string detect_linux_release() const;
    std::string detect_distro_code() const;
    std::string os_pretty_name() const;
    bool is_debian_family() const;
    bool is_redhat_family() const;
    bool is_arch_family() const;
    bool is_alpine() const;

    /** 添加 Docker GPG 密钥和软件源 (Debian)。 */
    bool add_docker_debian_repo();

    /** TUI 让用户选择镜像标签。 */
    std::string tui_input_tag();
};

} // namespace tmoe::domain
