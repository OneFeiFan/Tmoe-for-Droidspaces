#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {
    /** QEMU 虚拟机配置 (对应 Bash: tools/virtualization/qemu/startqemu 各全局变量)。 */
    struct QemuVmConfig {
        std::string name;
        std::string arch = "x86_64";
        std::string machine_type = "q35";
        bool kvm_enabled = true;
        std::string cpu_model = "host";
        int sockets = 1;
        int cores = 2;
        int threads = 1;
        int ram_mb = 2048;
        std::string boot_order = "cd";
        bool boot_menu = false;
        // 磁盘
        std::string virtio_disk_path;
        int64_t virtio_disk_size_gb = 10;
        std::string sata_disk_path;
        std::string cdrom_iso_path;
        // 网络
        bool net_enabled = true;
        std::string net_card = "virtio-net-pci";
        int vnc_port = 5901;
        bool vnc_password_enabled = false;
        std::string spice_port;
        // 高级
        std::string kernel_path;
        std::string initrd_path;
        std::string kernel_cmdline_append;
        bool uefi_enabled = false;
        std::string shared_dir_path;
        int tcp_host_fwd_port = 0;
        int tcp_guest_fwd_port = 0;
    };

    /** 虚拟化主管理器，对应 Bash 脚本:
     *   tools/virtualization/virt-menu
     *   tools/virtualization/qemu/*
     *   tools/virtualization/iso.sh
     *   tools/virtualization/vbox.sh
     *   tools/virtualization/wine32.sh
     *   tools/virtualization/docker/*
     */
    class VirtualizationManager {
    public:
        explicit VirtualizationManager(const TmoeConfig &cfg);

        // ── TUI 入口 ──
        /** 虚拟化主菜单 (whiptail TUI)。 */
        void run_virt_menu();

        // ── QEMU 虚拟机管理 ──
        /** 创建 QEMU 磁盘镜像。 */
        bool qemu_create_disk(std::string_view name, int64_t size_gb,
                              std::string_view format = "qcow2");

        /** 压缩 QCOW2 磁盘。 */
        bool qemu_compress_disk(std::string_view disk_path);

        /** 创建虚拟机配置并生成启动脚本。 */
        bool qemu_create(std::string_view name, int64_t disk_size_gb,
                         int ram_mb, int cpu_cores);

        /** 启动 QEMU 虚拟机。 */
        bool qemu_start(std::string_view name);

        /** 停止 QEMU 虚拟机。 */
        bool qemu_stop(std::string_view name);

        /** 列出运行中的 QEMU 虚拟机。 */
        std::vector<std::string> qemu_list() const;

        /** TUI 交互式创建虚拟机向导。 */
        bool qemu_tui_wizard();

        // ── ISO/磁盘镜像下载 ──
        /** 下载操作系统 ISO 文件。 */
        bool download_iso(std::string_view os_name);

        /** 下载预构建 QEMU 磁盘镜像。 */
        bool download_disk_image(std::string_view template_name);

        /** TUI ISO/镜像下载菜单。 */
        void run_iso_menu();

        // ── VirtualBox ──
        /** 安装 VirtualBox。 */
        bool install_virtualbox();

        /** 安装 VirtualBox Extension Pack。 */
        bool install_vbox_extension_pack();

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

        /** 检查 QEMU 是否可用。 */
        bool is_qemu_available() const;

        /** 获取 QEMU 虚拟机存储目录。 */
        fs::path qemu_dir() const;

    private:
        const TmoeConfig &cfg_;

        // ── ISO 下载 URL 映射 ──
        struct IsoInfo {
            std::string name;
            std::string url;
            std::string description;
        };

        static std::vector<IsoInfo> available_isos();

        static std::vector<std::pair<std::string, std::string> > available_disk_templates();

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
