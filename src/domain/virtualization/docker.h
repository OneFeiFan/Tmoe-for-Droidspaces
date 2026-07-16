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
        std::string name; // 如 "debian"
        std::string tag; // 如 "latest"
        std::string full_name() const { return name + ":" + tag; }
    };

    /** Docker 发行版注册项。 */
    struct DockerDistroEntry {
        std::string image; // 镜像名 (如 "debian")
        std::string label; // TUI 显示标签
        std::string tag1; // 主 tag
        std::string tag2; // 备选 tag
        std::string container; // 容器名
    };

    /** Docker 容器管理，对应 Bash 脚本:
     *   tools/virtualization/docker/docker_menu (1200行)
     *   tools/virtualization/docker/configure (99行)
     */
    class DockerManager {
    public:
        explicit DockerManager(const TmoeConfig &cfg);

        // ── Docker 安装 ──
        /** 安装 Docker CE (社区版)。 */
        bool install_docker_ce();

        /** 安装 Docker.io (Debian 仓库版)。 */
        bool install_docker_io();

        /** 安装 Docker CE 或 Docker.io（TUI选择）。 */
        bool install_docker_ce_or_io();

        /** 安装 Portainer Web 管理界面 (对应 Bash install_docker_portainer, 默认端口39080)。 */
        bool install_docker_portainer();

        // ── 镜像管理 ──
        /** 拉取 Docker 镜像。 */
        bool pull_image(std::string_view image, std::string_view tag = "latest");

        /** 列出本地 Docker 镜像。 */
        std::vector<DockerImageInfo> list_images() const;

        /** 选择并拉取 Linux 发行版镜像 (完整 TUI，含 17 种发行版)。 */
        void choose_gnu_linux_docker_images();

        /** TUI 拉取发行版镜像（旧版简易列表）。 */
        bool tui_pull_distro_image();

        // ── 容器管理 ──
        /** 运行 Docker 容器（特权模式 + 持久化挂载）。 */
        bool run_container(std::string_view image, std::string_view name,
                           std::string_view mount_path = "",
                           int host_port = 0, int container_port = 0);

        /** 运行特殊 tag Docker 容器。 */
        bool run_special_tag_docker_container(const std::string &docker_name,
                                              const std::string &docker_tag,
                                              const std::string &container_name,
                                              bool systemd = false);

        /** 列出运行中的 Docker 容器。 */
        std::vector<std::string> list_containers(bool all = false) const;

        /** 停止并删除容器。 */
        bool remove_container(std::string_view name);

        /** 删除容器 + 镜像。 */
        bool delete_container_and_image(const std::string &docker_name,
                                        const std::string &docker_tag,
                                        const std::string &docker_tag2 = "",
                                        const std::string &container_name = "");

        /** 连接 Docker 容器 (docker attach / exec)。 */
        bool docker_attach_container(const std::string &container_name,
                                     const std::string &docker_name,
                                     const std::string &docker_tag);

        /** 重置容器（删除 + 重拉 + 重建）。 */
        bool reset_docker_container(const std::string &docker_name,
                                    const std::string &docker_tag,
                                    const std::string &container_name);

        // ── 导入/导出 ──
        /** 导出容器为 tar 文件。 */
        bool export_container(std::string_view name, std::string_view output_path);

        /** 导出容器 TUI 菜单（含容器列表）。 */
        void export_docker_image_menu();

        /** 从 tar 文件导入为 Docker 镜像。 */
        bool import_image(std::string_view tar_path,
                          std::string_view image_name,
                          std::string_view tag = "latest");

        /** 导入 Docker 镜像 TUI 菜单（含文件管理）。 */
        void import_docker_image_menu();

        // ── Docker 配置 ──
        /** 配置 Docker 镜像源 (增强子菜单：4镜像/编辑/软件源/删除)。 */
        bool configure_mirror();

        /** Docker 镜像源子菜单（含编辑daemon.json/软件源/删除docker-ce.list）。 */
        void docker_mirror_source();

        /** 添加当前用户到 docker 组。 */
        bool add_user_to_docker_group();

        /** 检查 Docker 是否已安装并可运行。 */
        bool is_docker_available() const;

        // ── 信息输出 ──
        /** 显示 Docker 使用说明。 */
        void tmoe_docker_readme(const std::string &container_name) const;

        /** 自定义容器标签 TUI。 */
        std::string custom_docker_container_tag(const std::string &docker_name,
                                                const std::string &container_name);

    private:
        const TmoeConfig &cfg_;

        // ── 内部辅助 ──
        std::string detect_linux_release() const;

        std::string detect_distro_code() const;

        std::string os_pretty_name() const;

        bool is_debian_family() const;

        bool is_redhat_family() const;

        bool is_arch_family() const;

        bool is_alpine() const;

        std::string detect_true_arch() const;

        /** 添加 Docker GPG 密钥和软件源 (Debian)。 */
        bool add_docker_debian_repo();

        /** 增强版 Debian Docker 仓库添加（含 TUI 源选择）。 */
        bool debian_add_docker_gpg();

        /** TUI 让用户选择镜像标签。 */
        std::string tui_input_tag();

        /** 确保 Docker 服务正在运行。 */
        void ensure_docker_running();

        /** 检查 Docker 是否已安装（未安装则提示安装）。 */
        bool check_docker_installation();

        /** 容器管理子菜单（类型1: 1标签+2标签+自定义+systemd+连接+说明+重置+删除）。 */
        void docker_management_menu_01(const std::string &docker_name,
                                       const std::string &container_name,
                                       const std::string &tag1,
                                       const std::string &tag2);

        /** 容器管理子菜单（类型2: 双镜像名）。 */
        void docker_management_menu_02(const std::string &docker_name,
                                       const std::string &docker_name2,
                                       const std::string &container_name,
                                       const std::string &tag1);

        /** 容器管理子菜单（类型3: 单标签+自定义+systemd+连接+说明+重置+删除）。 */
        void docker_management_menu_03(const std::string &docker_name,
                                       const std::string &container_name,
                                       const std::string &tag1);

        /** 初始化 Docker 挂载目录。 */
        void docker_init();

        /** Proot 环境警告。 */
        void proot_warning_check() const;

        // ── 状态 ──
        std::string current_docker_name_;
        std::string current_container_name_;
        std::string current_docker_tag1_;
        std::string current_docker_tag2_;
        std::string current_docker_name2_;
        int current_mgt_menu_type_ = 1;
    };
} // namespace tmoe::domain
