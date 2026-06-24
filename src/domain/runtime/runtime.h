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

    /** PRoot 运行时策略（对应 old/share/container/proot/startup）。
     *  完整的 PRoot 配置：二进制选择、loader、EXA 仿真、挂载系统。
     */
    class ProotRuntime : public IContainerRuntime {
    public:
        struct ProotConfig {
            // ── 用户与工作目录 ──
            std::string proot_user{"root"};
            std::string home_dir{"default"};
            std::string work_dir{"default"};

            // ── 二进制与兼容性 ──
            bool dot_net_6_compatible_mode{false};

            std::string proot_bin{"default"}; // default|system|termux|32|compatibility|path
            std::string proot_compatible_mode_bin{"${TMOE_LINUX_DIR}/lib/data/data/com.termux/files/usr/bin/proot"};
            std::string proot_32_termux_bin{"${TMOE_LINUX_DIR}/lib32/data/data/com.termux/files/usr/bin/proot"};

            bool share_proot_loader{false};
            std::string proot_libexec_loader{"default"};
            std::string proot_32_termux_loader{
                "${TMOE_LINUX_DIR}/lib32/data/data/com.termux/files/usr/libexec/proot/loader"
            };
            std::string compatible_mode_loader{
                "${TMOE_LINUX_DIR}/lib/data/data/com.termux/files/usr/libexec/proot/loader"
            };

            std::string ld_lib_path{"default"};
            std::string proot_32_termux_ld_lib_path{"${TMOE_LINUX_DIR}/lib32/data/data/com.termux/files/usr/lib"};
            std::string compatible_mode_ld_lib_path{"${TMOE_LINUX_DIR}/lib/data/data/com.termux/files/usr/lib"};

            // ── proot 行为标志 ──
            bool kill_on_exit{true};
            bool proot_sysvipc{true};
            bool proot_l{true};
            bool proot_h{false};
            bool proot_p{false};
            bool fake_kernel{false};
            std::string kernel_release{"5.10.105-3-cloud-"};
            bool link_to_symlink{true};
            bool proot_debug{false};
            int verbose_level{2};
            bool old_android_version_compatibility_mode{false};

            std::string host_distro; // "Android" | "linux"
            std::string host_arch;
            std::string container_distro;
            std::string container_name;
            std::string container_arch{"amd64"};

            // ── EXA 仿真 ──
            bool exa_enabled{false};
            std::string exa_path{"${TMOE_LINUX_DIR}/lib32/usr/bin"};
            std::string exa_prefix; // 运行时用 rootfs 填充
            std::string vfs_hacks{"tlsasws,tsi,spd"};
            std::string socket_path_suffix;
            std::string vpaths_list{"/dev/null"};
            std::string vfs_kind{"guest-first"};

            // ── 挂载开关 ──
            std::string mount_sd; // 空=从配置文件读取
            std::string sd_dir_0{"/storage/self/primary/Download"};
            std::string sd_dir_1{"/sdcard/Download"};
            std::string sd_dir_2{"/storage/emulated/0/Download"};
            std::string sd_dir_3{"${HOME}/sd/Download"};
            std::string sd_dir_4{"${HOME}/Downloads"};
            std::string sd_dir_5{"${HOME}/Download"};
            std::string sd_mount_point{"/media/sd"};

            std::string mount_termux;
            std::string termux_dir{"/data/data/com.termux/files"};
            std::string termux_mount_point{"/media/termux"};

            std::string mount_tf;
            std::string tf_card_link{"${HOME}/storage/external-1"};
            std::string tf_mount_point{"/media/tf"};

            std::string mount_storage;
            std::string storage_dir{"/storage"};
            std::string storage_mount_point{"/storage"};

            bool mount_gitstatus{true};
            std::string gitstatus_dir;
            std::string gitstatus_mount_point{"/root/.cache/gitstatus"};

            bool mount_tmp{false};
            std::string tmp_source_dir;
            std::string tmp_mount_point{"/tmp"};

            bool mount_system{true};
            std::string system_dir{"/system"};

            bool mount_apex{true};
            std::string apex_dir{"/apex"};

            bool mount_sys{false};
            std::string sys_dir{"/sys"};

            bool mount_dev{true};
            std::string dev_dir{"/dev"};
            bool mount_shm_to_tmp{true};
            bool mount_urandom_to_random{true};
            bool mount_dev_fd{true};
            bool mount_dev_stdin{true};
            bool mount_dev_stdout{true};
            bool mount_dev_stderr{true};
            bool mount_dev_tty{true};

            bool mount_proc{true};
            std::string proc_dir{"/proc"};
            bool fake_proot_proc{true};

            bool mount_cap_last_cap{true};
            std::string cap_last_cap_source{"/dev/null"};
            std::string cap_last_cap_mount_point{"/proc/sys/kernel/cap_last_cap"};

            // ── 自定义挂载（最多12个）──
            struct CustomMount {
                bool enabled{false};
                std::string source;
                std::string dest;
            };

            CustomMount custom_mounts[12];

            // ── Shell ──
            std::vector<std::string> default_login_shells{"/bin/zsh", "/bin/fish", "/bin/bash", "/bin/ash", "/bin/su"};
            std::string login_shell_arg{"-l"};

            // ── 环境与配置 ──
            bool load_env_file{true};
            std::string container_env_file;
            bool load_proot_conf{true};
            std::string proot_conf_file;

            std::string tmoe_locale_file;
            std::string default_shell_conf;
            std::string proc_fd_path{"/proc/self/fd"};
            std::string host_name_file;
        };

        ProotRuntime() = default;

        explicit ProotRuntime(const ProotConfig &cfg) : config_(cfg) {
        }

        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;

    private:
        ProotConfig config_;

        void apply_proot_args(const Container &container, std::vector<std::string> &args) const;
    };

    /** Chroot 运行时策略（对应 old/share/container/chroot/startup）。
     *  支持多 chroot 二进制、unshare、复杂挂载逻辑。
     */
    class ChrootRuntime : public IContainerRuntime {
    public:
        /** Chroot 配置结构体（对应 Bash 脚本变量） */
        struct ChrootConfig {
            std::string chroot_bin{"system"}; // system|termux|busybox|toybox|termux-busybox|path
            std::string mount_bin{"system"}; // mount 二进制选择
            std::string unshare_bin{"termux"}; // unshare 二进制选择
            std::string rootfs_path;
            std::string chroot_user{"root"};
            std::string home_dir{"default"};
            bool old_android_compat{false};

            // ── 配置文件加载 ──
            bool load_chroot_conf{true};
            std::string chroot_conf_file;
            bool load_env_file{true};
            std::string container_env_file;
            std::string tmoe_locale_file;
            std::string default_shell_conf;

            // 挂载开关
            bool mount_itself{true};
            bool mount_dev{true};
            bool mount_proc{true};
            bool mount_sys{true};
            bool mount_dev_pts{true};
            bool mount_dev_shm{true};
            bool fix_dev_link{true};
            bool mount_gitstatus{false};
            std::string gitstatus_dir;
            bool mount_tmp{false};
            std::string tmp_source_dir{"/tmp"};
            std::string tmp_mount_point{"/tmp"};

            // SD/Termux/TF 挂载
            std::string mount_sd; // 空=从配置文件读取
            std::string sd_dir_0{"/data/media/0/Download"};
            std::string sd_dir_1{"/storage/self/primary/Download"};
            std::string sd_dir_2{"/sdcard/Download"};
            std::string sd_dir_3{"${HOME}/sd/Download"};
            std::string sd_dir_4{"${HOME}/Downloads"};
            std::string sd_dir_5{"${HOME}/Download"};
            std::string sd_mount_point{"/media/sd"};

            std::string mount_termux; // 空=从配置文件读取
            std::string termux_dir{"/data/data/com.termux/files/home"};
            std::string termux_mount_point{"/media/termux"};

            // TF 卡挂载
            std::string mount_tf;
            std::string tf_card_link{"${HOME}/storage/external-1"};
            std::string tf_mount_point{"/media/tf"};

            // 挂载配置文件路径
            std::string sd_conf_file;
            std::string termux_conf_file;
            std::string tf_conf_file;

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

            // /proc/self/fd 路径（用于 /dev/fd 等符号链接）
            std::string proc_fd_path{"/proc/self/fd"};

            // host 检查
            std::string host_distro; // "Android" | "linux"
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
            std::string sd_conf_file;
            std::string sd_dir_0{"/media/sd/Download"};
            std::string sd_dir_1{"${HOME}/sd/Download"};
            std::string sd_dir_2{"/mnt/sdcard/Download"};
            std::string sd_dir_3{"/storage/emulated/0/Download"};
            std::string sd_dir_4{"/sdcard/Download"};
            std::string sd_dir_5{"${HOME}/Download"};
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

    // 待办: class DockerRuntime : public IContainerRuntime { ... };
} // namespace tmoe::domain
#endif //RUNTIME_H
