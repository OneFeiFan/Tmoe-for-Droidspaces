#ifndef RUNTIME_H
#define RUNTIME_H
#pragma once
#include <string>
#include "core/launch_context.h"

namespace tmoe::domain {
    class Container; // 前置声明

    /** 容器运行时抽象接口（策略模式）。
     *  各引擎自行处理隔离环境的搭建与销毁。
     */
    class IContainerRuntime {
    public:
        virtual ~IContainerRuntime() = default;

        /** 为指定容器生成启动命令字符串。 */
        virtual std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const =
        0;

        /** 启动容器。 */
        virtual bool start(const Container &container, const LaunchContext *ctx = nullptr) = 0;

        /** 停止容器。 */
        virtual bool stop(const Container &container) = 0;
    };

    /** PRoot 运行时策略。 */
    class ProotRuntime : public IContainerRuntime {
    public:
        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;
    };

    /** Chroot 运行时策略（对应 old/share/container/chroot/startup）。
     *  支持多 chroot 二进制、unshare、复杂挂载逻辑。
     */
    class ChrootRuntime : public IContainerRuntime {
    public:
        /** Chroot 配置结构体（对应 Bash 脚本变量） */
        struct ChrootConfig {
            std::string chroot_bin{"system"}; // system|termux|busybox|toybox|path
            std::string mount_bin{"system"}; // mount 二进制选择
            std::string unshare_bin{"termux"}; // unshare 二进制选择
            std::string rootfs_path;
            std::string chroot_user{"root"};
            std::string home_dir{"default"};
            bool old_android_compat{false};

            // 挂载开关
            bool mount_itself{true};
            bool mount_dev{true};
            bool mount_proc{true};
            bool mount_sys{true};
            bool mount_dev_pts{true};
            bool mount_dev_shm{true};
            bool fix_dev_link{true};
            bool mount_gitstatus{false}; // 挂载 etc/gitstatus
            bool mount_tmp{false};
            std::string tmp_source_dir{"/tmp"};
            std::string tmp_mount_point{"/tmp"};

            // SD/Termux/TF 挂载
            std::string mount_sd; // 空=从配置文件读取
            std::string sd_dir_0{"/data/media/0/Download"};
            std::string sd_dir_1{"/storage/self/primary/Download"};
            std::string sd_dir_2{"/sdcard/Download"};
            std::string sd_dir_3{"/mnt/sdcard/Download"};
            std::string sd_dir_4{"/storage/emulated/0/Download"};
            std::string sd_mount_point{"/media/sd"};

            std::string mount_termux; // 空=从配置文件读取
            std::string termux_dir{"/data/data/com.termux/files/home"};
            std::string termux_mount_point{"/media/termux"};

            // 自定义挂载点（最多10个）
            struct CustomMount {
                bool enabled{false};
                std::string source;
                std::string dest;
                bool readonly{false};
            };

            CustomMount custom_mounts[10];

            // unshare 配置
            bool unshare_enabled{false};
            bool unshare_ipc{false};
            bool unshare_pid{false};
            bool unshare_uts{false};
            bool unshare_mount{false};
            bool kill_child{false};
            std::string kill_child_signame{"SIGKILL"};
            bool share_proc{true};

            // Shell 配置
            std::vector<std::string> default_login_shells{"/bin/zsh", "/bin/fish", "/bin/bash", "/bin/ash", "/bin/su"};
            std::string login_shell_arg{"-l"};

            // 环境文件
            bool load_env_file{true};
            std::string container_env_file;

            // /proc/self/fd 路径（用于 /dev/fd 等符号链接）
            std::string proc_fd_path{"/proc/self/fd"};
        };

        ChrootRuntime() = default;

        explicit ChrootRuntime(const ChrootConfig &cfg) : config_(cfg) {
        }

        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;

    private:
        ChrootConfig config_;

        std::string detect_chroot_bin() const;

        std::string detect_mount_bin() const;

        std::string detect_unshare_bin() const;

        void do_mounts(const Container &container) const;

        void fix_dev_links(const Container &container) const;

        std::string detect_shell(const Container &container) const;
    };

    /** Systemd-nspawn 运行时策略（对应 old/share/container/nspawn/startup）。
     *  支持 systemd-nspawn 启动、boot 模式、GPU 直通、X11 socket。
     */
    class NspawnRuntime : public IContainerRuntime {
    public:
        struct NspawnConfig {
            std::string nspawn_bin{"system"}; // system|termux|prefix|path
            std::string rootfs_path;
            std::string chroot_user{"root"};
            bool systemd_boot{false};

            // 设备挂载
            bool mount_dev_dri{true};
            std::string dev_dri{"/dev/dri"};
            bool mount_nvidia_gpu{true};
            std::string nvidia_gpu{"/dev/nvidia0"};
            std::string nvidia_ctl{"/dev/nvidiactl"};
            bool mount_x11_unix{false};
            bool x11_unix_ro{true};
            std::string x11_unix_dir{"/tmp/.x11-unix"};

            // SD 挂载
            std::string mount_sd; // 空=从配置文件读取
            std::string sd_dir_0{"/media/sd/Download"};
            std::string sd_dir_1{"${HOME}/sd/Download"};
            std::string sd_dir_2{"/mnt/sdcard/Download"};
            std::string sd_dir_3{"/storage/emulated/0/Download"};
            std::string sd_dir_4{"/sdcard/Download"};
            std::string sd_mount_point{"/media/sd"};
            bool mount_docker_dir{false};
            std::string docker_mount_point{"/media/docker"};

            // 自定义挂载点（最多12个，对应 Bash 版 NUM_OF_MOUNTS=12）
            struct CustomMount {
                bool enabled{false};
                std::string source;
                std::string dest;
                bool readonly{false};
            };

            CustomMount custom_mounts[12];

            // 环境
            bool load_env_file{true};
            std::string container_env_file;
            bool load_nspawn_conf{false};
            std::string nspawn_conf_file;
        };

        NspawnRuntime() = default;

        explicit NspawnRuntime(const NspawnConfig &cfg) : config_(cfg) {
        }

        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;

    private:
        NspawnConfig config_;

        std::string detect_nspawn_bin() const;

        void build_nspawn_args(const Container &container, const LaunchContext *ctx,
                               std::vector<std::string> &args) const;
    };

    /** QEMU 虚拟机运行时策略（对应 old/tools/virtualization/qemu/startqemu）。
     *  完整 QEMU 虚拟机配置：机器类型、CPU、内存、存储、网络、GPU、VNC/SPICE。
     */
    class QemuRuntime : public IContainerRuntime {
    public:
        struct QemuConfig {
            // 机器属性
            std::string machine_accel{"kvm:tcg"};
            bool enable_kvm{true};
            std::string machine_type{"q35"}; // q35|pc
            std::string qemu_name{"tmoe-qemu"};
            std::string rtc_base{"localtime"};
            bool uefi_enabled{false};
            std::string uefi_code_pflash{"/usr/share/OVMF/OVMF_CODE.fd"};
            std::string uefi_vars_pflash;

            // CPU
            std::string cpu_model{"max"};
            std::string cpu_id_flags{"hle=off,rtm=off"};

            // SMP
            int cpus{2};
            int sockets{2};
            int cores{1};
            int threads{1};

            // 内存
            std::string memory_allocation{"auto"}; // auto|num
            int memory_size{1024}; // MB
            int max_memory{4096}; // MB
            int memory_reduced{256}; // MB

            // 引导
            std::string boot_order{"cd"};
            bool boot_menu{true};
            bool custom_kernel{false};
            std::string kernel_file;
            std::string initrd_file;
            std::string kernel_append;

            // 存储设备（对应 Bash 版变量）
            struct StorageDevice {
                bool enabled{false};
                std::string format{"qcow2"};
                std::string path;
                std::string bootindex;
            };

            StorageDevice virtio_disk[2];
            StorageDevice sata_disk[2];
            StorageDevice usb_disk[2];
            StorageDevice floppy_disk[2];
            StorageDevice cdrom_disk[2];

            // VirtIO 9p 共享
            bool virtio_9p_enabled{true};
            std::string virtio_9p_path;

            // 声卡
            bool sound_card_enabled{true};
            std::string sound_card{"intel-hda"}; // intel-hda|AC97|ES1370
            bool sound_card_02_enabled{true};
            std::string sound_card_02{"AC97"};
            std::string audio_addr{"127.0.0.1:4713"};
            bool wayland_remote_pulse{false};

            // 网卡
            struct NetDevice {
                bool enabled{true};
                std::string model{"virtio-net-pci"}; // virtio-net-pci|e1000|rtl8139
                std::string mac;
                bool tcp_fwd_enabled{true};
                std::string tcp_fwd; // "host_port:guest_port,..."
                bool udp_fwd_enabled{false};
                std::string udp_fwd;
            };

            NetDevice net_devices[3];

            // GPU
            std::string gpu_model{"qxl-vga"}; // qxl-vga|virtio-vga|VGA|bochs-display
            bool virtio_3d_accel{false};
            std::string gpu_ram_size{"67108864"};
            std::string gpu_vram_size{"67108864"};
            int gpu_vgamem_mb{16};

            // 远程桌面
            std::string connection_type{"vnc"}; // vnc|spice|x11|remote-x11
            std::string remote_x11_addr{"127.0.0.1:0"};
            int vnc_port{5905};
            bool vnc_localhost{false};
            bool vnc_startup_prompt{true};
            bool vnc_password{false};

            // QEMU 二进制
            std::string qemu_bin_path{"/usr/bin"};
            std::string qemu_bin_arch{"x86_64"}; // x86_64|i386
        };

        QemuRuntime() = default;

        explicit QemuRuntime(const QemuConfig &cfg) : config_(cfg) {
        }

        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;

    private:
        QemuConfig config_;

        std::string detect_qemu_bin() const;

        void build_qemu_args(const QemuConfig &cfg, const Container &container, const LaunchContext *ctx,
                             std::vector<std::string> &args) const;

        void apply_memory_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        void apply_storage_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        void apply_network_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        void apply_gpu_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        void apply_sound_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        void apply_vnc_config(const QemuConfig &cfg, std::vector<std::string> &args) const;

        static int get_auto_memory_size(int reduced, int max_mem);
    };

    // 待办: class DockerRuntime : public IContainerRuntime { ... };
} // namespace tmoe::domain
#endif //RUNTIME_H
