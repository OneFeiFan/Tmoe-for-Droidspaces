#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {
    /** 虚拟化主管理器，对应 Bash 脚本:
     *   tools/virtualization/virt-menu
     *   tools/virtualization/iso.sh
     *   tools/virtualization/wine32.sh
     *   tools/virtualization/docker/*
     */
    class VirtualizationManager {
    public:
        explicit VirtualizationManager(const TmoeConfig &cfg);

        // ── TUI 入口 ──
        /** 虚拟化主菜单。 */
        void run_virt_menu();

        /** 设置 Docker 子页回调（从主 Manager 注入）。 */
        void set_docker_callback(std::function<void()> cb) { docker_cb_ = std::move(cb); }

        // ── ISO 镜像下载 ──
        /** 下载操作系统 ISO 文件。 */
        bool download_iso(std::string_view os_name);

        /** TUI ISO 下载菜单。 */
        void run_iso_menu();

        // ── Wine ──
        /** 安装 Wine (WineHQ)。 */
        bool install_wine(std::string_view branch = "devel");

        /** 安装 Winetricks。 */
        bool install_winetricks();

        /** 安装 DXVK (DirectX → Vulkan)。 */
        bool install_dxvk();

        /** 安装 PlayOnLinux (图形化前端)。 */
        bool install_playonlinux();

        /** TUI Wine 管理菜单。 */
        void run_wine_menu();

        // ── 辅助 ──
        /** 安装 virt-manager GUI。 */
        bool install_virt_manager();

    private:
        const TmoeConfig &cfg_;
        std::function<void()> docker_cb_;

        // ── ISO 下载 URL 映射 ──
        struct IsoInfo {
            std::string name;
            std::string url;
            std::string description;
        };

        static std::vector<IsoInfo> available_isos();

        // ── Wine 版本信息 ──
        static std::vector<std::pair<std::string, std::string> > wine_branches();

        // ── 内部辅助 ──
        std::string os_release() const;

        bool is_debian() const;

        bool is_ubuntu() const;

        bool is_arch() const;

        fs::path vm_store_dir() const;

        void ensure_vm_dir() const;
    };
} // namespace tmoe::domain
