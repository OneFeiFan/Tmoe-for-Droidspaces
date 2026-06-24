#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include "domain/runtime/container.h"
#include "domain/runtime/runtime.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/i18n.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    using tmoe::Executor; // Executor 定义在 namespace tmoe

    // ── 工具函数 ──

    /** 检测给定目标路径是否已挂载（对应 Bash 的 detect_mount） */
    static bool is_mounted(const std::string &target) {
        // Bash 原版：grep -qs " ${MOUNT_DIR%/} " /proc/mounts
        // 关键：在模式前后加空格，避免 /mnt/abc 误匹配 /mnt/abc-def
        std::string t = target;
        // 移除尾部斜杠（对应 Bash 的 ${var%/}）
        while (!t.empty() && t.back() == '/') t.pop_back();
        std::string cmd = "grep -qs ' " + t + " ' /proc/mounts";
        return Executor::shell(cmd).ok();
    }

    /** 执行挂载（对应 Bash 的 mount_01 / mount_ro_or_rw） */
    static void do_mount(const std::string &mount_bin,
                         const std::string &source,
                         const std::string &target,
                         bool readonly,
                         const std::string &prefix) {
        std::string cmd = prefix;
        if (!cmd.empty()) cmd += " ";
        cmd += mount_bin;
        if (readonly)
            cmd += " -o bind,ro";
        else
            cmd += " -o bind";
        cmd += " " + source + " " + target;
        Executor::shell(cmd);
    }

    /** 以 root 权限创建目录（对应 Bash 的 mkdir_01 / mkdir_dst） */
    static void mkdirp_as_root(const std::string &dir, const std::string &prefix) {
        std::string cmd = prefix;
        if (!cmd.empty()) cmd += " ";
        cmd += "mkdir -p " + dir;
        Executor::shell(cmd);
    }

    /** 获取当前非 root 用户可用的特权前缀（sudo 或 su -c） */
    static std::string get_root_prefix() {
        if (std::getenv("LINUX_DISTRO") != nullptr &&
            std::string(std::getenv("LINUX_DISTRO")) != "Android") {
            if (std::getenv("USER") != nullptr &&
                std::string(std::getenv("USER")) != "root") {
                if (Executor::shell("command -v sudo").ok()) {
                    return "sudo";
                } else {
                    return "su -c";
                }
            }
        }
        return "";
    }

    // ── ChrootRuntime 实现 ──

    std::string ChrootRuntime::detect_chroot_bin() const {
        const auto &c = config_;
        // 旧 Android 兼容模式：强制用 termux chroot
        if (c.old_android_compat) {
            const char *prefix = std::getenv("PREFIX");
            return std::string(prefix ? prefix : "") + "/bin/chroot";
        }
        if (c.chroot_bin == "system" || c.chroot_bin == "default" || c.chroot_bin.empty()) {
            return "chroot";
        } else if (c.chroot_bin == "termux" || c.chroot_bin == "prefix") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/chroot";
        } else if (c.chroot_bin == "busybox") {
            return "busybox chroot";
        } else if (c.chroot_bin == "toybox") {
            return "toybox chroot";
        } else if (c.chroot_bin == "system-toybox") {
            return "/system/bin/toybox chroot";
        } else if (c.chroot_bin == "termux-busybox") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/busybox chroot";
        }
        return c.chroot_bin;
    }

    std::string ChrootRuntime::detect_mount_bin() const {
        const auto &c = config_;
        if (c.mount_bin == "system" || c.mount_bin == "default" || c.mount_bin.empty()) {
            return "mount";
        } else if (c.mount_bin == "termux" || c.mount_bin == "prefix") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/mount";
        } else if (c.mount_bin == "busybox") {
            return "busybox mount";
        } else if (c.mount_bin == "toybox") {
            return "toybox mount";
        } else if (c.mount_bin == "system-toybox") {
            return "/system/bin/toybox mount";
        } else if (c.mount_bin == "termux-busybox") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/busybox mount";
        }
        return c.mount_bin;
    }

    std::string ChrootRuntime::detect_unshare_bin() const {
        const auto &c = config_;
        if (c.unshare_bin == "system" || c.unshare_bin == "default" || c.unshare_bin.empty()) {
            return "unshare";
        } else if (c.unshare_bin == "termux" || c.unshare_bin == "prefix") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/unshare";
        }
        return c.unshare_bin;
    }

    // ── 核心挂载逻辑（对应 Bash 的 start_tmoe_gnu_linux_chroot_container 挂载部分） ──

    void ChrootRuntime::do_mounts(const Container &container) const {
        std::string prefix = get_root_prefix();
        std::string mount_bin = detect_mount_bin();

        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;

        // 1. 自挂载（arch 自行处理，对应 Bash 的 MOUNT_ITSELF）
        if (config_.mount_itself) {
            if (!is_mounted(rootfs + "/")) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += mount_bin + " --rbind " + rootfs + " " + rootfs + "/";
                Executor::shell(cmd);
                cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += mount_bin + " -o remount,exec,suid,relatime,dev " + rootfs;
                Executor::shell(cmd);
            }
        }

        // 2. tmp 挂载
        if (config_.mount_tmp) {
            std::string target = rootfs + config_.tmp_mount_point;
            if (!is_mounted(target)) {
                mkdirp_as_root(config_.tmp_mount_point, prefix);
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += mount_bin + " -o bind " + config_.tmp_source_dir + " " + target;
                Executor::shell(cmd);
            }
        }

        // 3. 标准目录挂载（dev / proc / sys / dev/pts / dev/shm / etc/gitstatus）
        struct DirMount {
            const char *dir;
            bool enable;
            const char *extra_opts;
        };
        DirMount dirs[] = {
            {"dev", config_.mount_dev, nullptr},
            {"proc", config_.mount_proc, "rw,nosuid,nodev,noexec,relatime -t proc"},
            {"sys", config_.mount_sys, "rw,nosuid,nodev,noexec,relatime -t sysfs"},
            {"dev/pts", config_.mount_dev_pts, "rw,nosuid,noexec,relatime,gid=5,mode=620,ptmxmode=000 -t devpts"},
            {"dev/shm", config_.mount_dev_shm, "rw,nosuid,nodev,mode=1777 -t tmpfs"},
            {"etc/gitstatus", config_.mount_gitstatus, nullptr},
        };

        for (const auto &dm: dirs) {
            if (!dm.enable) continue;
            std::string target = rootfs + "/" + dm.dir;
            if (is_mounted(target)) continue;

            mkdirp_as_root(dm.dir, prefix);

            std::string cmd = prefix;
            if (!cmd.empty()) cmd += " ";
            cmd += mount_bin;

            if (dm.extra_opts != nullptr && dm.extra_opts[0] != '\0') {
                // 有特殊挂载选项（proc / sys / dev/pts / dev/shm）
                if (std::string(dm.dir) == "proc" && !config_.unshare_enabled) {
                    cmd += " -o " + std::string(dm.extra_opts) + " " + dm.dir + " " + target;
                } else if (std::string(dm.dir) != "proc") {
                    cmd += " -o " + std::string(dm.extra_opts) + " " + dm.dir + " " + target;
                }
            } else {
                cmd += " -o bind " + std::string(dm.dir) + " " + target;
            }
            Executor::shell(cmd);
        }

        // 4. 自定义挂载点（对应 Bash 的 mount[] / mount_src[] / mount_dst[] / mount_ro[]）
        for (int i = 0; i < 10; i++) {
            const auto &m = config_.custom_mounts[i];
            if (!m.enabled) continue;
            if (m.source.empty() || m.dest.empty()) continue;

            // 检查挂载点路径深度（Bash 版限制最多2级子目录）
            int slash_count = std::count(m.dest.begin(), m.dest.end(), '/');
            if (slash_count > 2) continue;

            std::string target = rootfs + m.dest;
            if (is_mounted(target)) continue;

            mkdirp_as_root(m.dest, prefix);
            do_mount(mount_bin, m.source, target, m.readonly, prefix);
        }

        // 5. SD 卡挂载（对应 Bash 的 sd 部分）
        if (config_.mount_sd.empty() || config_.mount_sd == "true") {
            bool sd_mounted = false;
            std::string sd_mount_point_path = rootfs + config_.sd_mount_point;
            mkdirp_as_root(config_.sd_mount_point, prefix);

            const char *sd_dirs[] = {
                config_.sd_dir_0.c_str(),
                config_.sd_dir_1.c_str(),
                config_.sd_dir_2.c_str(),
                config_.sd_dir_3.c_str(),
                config_.sd_dir_4.c_str(),
                config_.sd_dir_5.c_str(),
                nullptr
            };

            for (int i = 0; sd_dirs[i] != nullptr; i++) {
                if (sd_dirs[i][0] == '\0') continue;
                if (Executor::shell("test -d " + std::string(sd_dirs[i])).ok()) {
                    std::string cmd = prefix;
                    if (!cmd.empty()) cmd += " ";
                    cmd += mount_bin + " -o bind " + std::string(sd_dirs[i]) + " " + sd_mount_point_path;
                    Executor::shell(cmd);
                    sd_mounted = true;
                    break;
                }
            }
        }

        // 6. Termux 目录挂载（对应 Bash 的 termux 部分）
        if (config_.mount_termux.empty() || config_.mount_termux == "true") {
            if (!config_.termux_dir.empty()) {
                std::string target = rootfs + config_.termux_mount_point;
                mkdirp_as_root(config_.termux_mount_point, prefix);
                if (Executor::shell("test -d " + config_.termux_dir).ok()) {
                    std::string cmd = prefix;
                    if (!cmd.empty()) cmd += " ";
                    cmd += mount_bin + " -o bind " + config_.termux_dir + " " + target;
                    Executor::shell(cmd);
                }
            }
        }

        // 7. TF 卡挂载（对应 Bash 的 MOUNT_TF 部分）
        if (config_.mount_tf.empty() || config_.mount_tf == "true") {
            std::string tf_link = config_.tf_card_link;
            // 展开 ${HOME}
            const char *home = std::getenv("HOME");
            if (home) {
                std::string home_str(home);
                size_t pos = tf_link.find("${HOME}");
                if (pos != std::string::npos) tf_link.replace(pos, 7, home_str);
            }
            if (fs::is_symlink(tf_link)) {
                auto resolved = fs::read_symlink(tf_link);
                if (fs::exists(resolved)) {
                    std::string target = rootfs + config_.tf_mount_point;
                    mkdirp_as_root(config_.tf_mount_point, prefix);
                    std::string cmd = prefix;
                    if (!cmd.empty()) cmd += " ";
                    cmd += mount_bin + " -o bind " + resolved.string() + " " + target;
                    Executor::shell(cmd);
                }
            }
        }
    }

    void ChrootRuntime::fix_dev_links(const Container &container) const {
        if (!config_.fix_dev_link) return;

        std::string prefix = get_root_prefix();
        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;

        // PROC_FD_PATH 来自配置，默认为 "/proc/self/fd"（host 路径，在 chroot 内对应 /proc/self/fd）
        std::string proc_fd = "/proc/self/fd";
        if (!config_.proc_fd_path.empty()) {
            proc_fd = config_.proc_fd_path;
        }

        struct DevLink {
            const char *path;
            const char *target;
        };
        DevLink links[] = {
            {"/dev/fd", proc_fd.c_str()},
            {"/dev/stdin", (proc_fd + "/0").c_str()},
            {"/dev/stdout", (proc_fd + "/1").c_str()},
            {"/dev/stderr", (proc_fd + "/2").c_str()},
            {"/dev/tty0", "/dev/null"},
        };

        for (const auto &link: links) {
            std::string full_path = rootfs + link.path;
            if (!fs::exists(full_path) && !fs::is_symlink(full_path)) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "ln -s " + std::string(link.target) + " " + full_path + " 2>/dev/null";
                Executor::shell(cmd);
            }
        }
    }

    // ── Shell 检测（对应 Bash 的 DEFAULT_LOGIN_SHELL_* 检测） ──

    std::string ChrootRuntime::detect_shell(const Container &container) const {
        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;
        std::string shell;

        // 如果指定了非 root 用户，从 /etc/passwd 检测用户 shell
        if (!config_.chroot_user.empty() && config_.chroot_user != "root") {
            std::string cmd = "grep '^" + config_.chroot_user + ":' " + rootfs +
                              "/etc/passwd | awk -F ':' '{print $7}'";
            auto r = Executor::shell(cmd);
            if (r.ok() && !r.stdout_data.empty()) {
                shell = r.stdout_data;
                shell.erase(std::remove(shell.begin(), shell.end(), '\n'), shell.end());
                shell.erase(std::remove(shell.begin(), shell.end(), '\r'), shell.end());
                if (!shell.empty()) return shell;
            }
        }

        // 按优先级检测 shell（对应 Bash 的 DEFAULT_LOGIN_SHELL_*）
        for (const auto &s: config_.default_login_shells) {
            std::string full_path;
            if (s.find('/') == std::string::npos) {
                full_path = rootfs + "/bin/" + s;
            } else {
                full_path = rootfs + s;
            }
            if (fs::exists(full_path) || fs::is_symlink(full_path)) {
                shell = (s.find('/') == std::string::npos) ? ("/bin/" + s) : s;
                break;
            }
        }

        return shell.empty() ? "/bin/sh" : shell;
    }

    // ── 命令生成（对应 Bash 的 startup 脚本参数构建） ──

    std::string ChrootRuntime::generate_launch_cmd(const Container &container,
                                                   const LaunchContext *ctx) const {
        std::string chroot_bin = detect_chroot_bin();
        std::string unshare_bin = config_.unshare_enabled ? detect_unshare_bin() : "";

        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;

        std::vector<std::string> args;

        // 如果启用了 unshare，则 unshare 是主程序
        if (config_.unshare_enabled && !unshare_bin.empty()) {
            args.emplace_back(unshare_bin);

            if (config_.share_proc && config_.unshare_enabled) {
                // 需要 umount 旧 proc
                std::string prefix = get_root_prefix();
                if (fs::exists(rootfs + "/proc/stat")) {
                    std::string cmd = prefix;
                    if (!cmd.empty()) cmd += " ";
                    cmd += "umount -lf " + rootfs + "/proc || umount " + rootfs + "/proc";
                    Executor::shell(cmd);
                }
                args.emplace_back("--mount-proc");
            }
            if (config_.unshare_ipc) args.emplace_back("--ipc");
            if (config_.unshare_pid) args.emplace_back("--pid");
            if (config_.unshare_uts) args.emplace_back("--uts");
            if (config_.unshare_mount) args.emplace_back("--mount");
            if (config_.kill_child && !config_.kill_child_signame.empty()) {
                args.emplace_back("--kill-child=" + config_.kill_child_signame);
            }
            args.emplace_back("-R");
            args.emplace_back(rootfs);
            // chroot 作为 unshare 的参数
            args.emplace_back(chroot_bin);
        } else {
            args.emplace_back(chroot_bin);
            args.emplace_back(rootfs);
        }

        // 设置环境变量（对应 Bash 的 set -- "$@" "VAR=value"）
        args.emplace_back("/usr/bin/env");
        args.emplace_back("-i");

        // HOSTNAME（对应 Bash 的 HOST_NAME 部分）
        std::string hostname = "localhost";
        std::string hostname_file = rootfs + "/etc/hostname";
        if (fs::exists(hostname_file)) {
            std::ifstream hf(hostname_file);
            if (hf.is_open() && std::getline(hf, hostname)) {
                // 成功读取
            }
        } else if (Executor::shell("command -v hostname").ok()) {
            auto r = Executor::shell("hostname -f");
            if (r.ok() && !r.stdout_data.empty()) {
                hostname = r.stdout_data;
                hostname.erase(std::remove(hostname.begin(), hostname.end(), '\n'), hostname.end());
                hostname.erase(std::remove(hostname.begin(), hostname.end(), '\r'), hostname.end());
            }
        }
        args.emplace_back("HOSTNAME=" + hostname);

        // 环境变量（对应 Bash 版）
        args.emplace_back("TERM=xterm-256color");
        args.emplace_back("SDL_IM_MODULE=fcitx");
        args.emplace_back("XMODIFIERS=@im=fcitx");
        args.emplace_back("QT_IM_MODULE=fcitx");
        args.emplace_back("GTK_IM_MODULE=fcitx");
        args.emplace_back("TMOE_CHROOT=true");
        args.emplace_back("TMOE_PROOT=false");
        args.emplace_back("TMPDIR=/tmp");
        args.emplace_back("DISPLAY=:2");
        args.emplace_back("PULSE_SERVER=tcp:127.0.0.1:4713");

        // Shell
        std::string shell = detect_shell(container);
        args.emplace_back("SHELL=" + shell);

        // LANG（对应 Bash 的 TMOE_LOCALE_FILE）
        std::string locale_file = rootfs + "/usr/local/etc/tmoe-linux/locale.txt";
        if (fs::exists(locale_file)) {
            std::ifstream lf(locale_file);
            std::string lang;
            if (lf.is_open() && std::getline(lf, lang)) {
                lang.erase(std::remove(lang.begin(), lang.end(), '\n'), lang.end());
                lang.erase(std::remove(lang.begin(), lang.end(), '\r'), lang.end());
                args.emplace_back("LANG=" + lang);
            } else {
                args.emplace_back("LANG=en_US.UTF-8");
            }
        } else {
            args.emplace_back("LANG=en_US.UTF-8");
        }

        // HOME（对应 Bash 的 HOME_DIR 部分）
        std::string home_dir;
        if (config_.home_dir == "default" || config_.home_dir.empty()) {
            if (config_.chroot_user.empty() || config_.chroot_user == "root") {
                home_dir = "/root";
            } else {
                std::string cmd = "grep '^" + config_.chroot_user + ":' " + rootfs +
                                  "/etc/passwd | awk -F ':' '{print $6}'";
                auto r = Executor::shell(cmd);
                if (r.ok() && !r.stdout_data.empty()) {
                    home_dir = r.stdout_data;
                    home_dir.erase(std::remove(home_dir.begin(), home_dir.end(), '\n'), home_dir.end());
                    home_dir.erase(std::remove(home_dir.begin(), home_dir.end(), '\r'), home_dir.end());
                }
                if (home_dir.empty()) home_dir = "/home/" + config_.chroot_user;
            }
        } else {
            home_dir = config_.home_dir;
        }
        args.emplace_back("HOME=" + home_dir);
        args.emplace_back("USER=" + config_.chroot_user);

        // PATH（含 CONTAINER_BIN_PATH 前缀，对应 Bash 版）
        std::string container_bin_path;
        if (config_.load_env_file && !config_.container_env_file.empty() && fs::exists(config_.container_env_file)) {
            std::ifstream ef(config_.container_env_file);
            std::string line;
            while (std::getline(ef, line)) {
                if (line.rfind("CONTAINER_BIN_PATH=", 0) == 0) {
                    container_bin_path = line.substr(19);
                    if (!container_bin_path.empty() && container_bin_path.back() == '\r')
                        container_bin_path.pop_back();
                    break;
                }
            }
        }
        std::string base_path;
        if (config_.chroot_user == "root" || config_.chroot_user.empty()) {
            base_path = "/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games";
        } else {
            base_path = "/usr/local/bin:/bin:/usr/bin:/usr/games:/usr/local/games";
        }
        if (!container_bin_path.empty())
            args.emplace_back("PATH=" + container_bin_path + ":" + base_path);
        else
            args.emplace_back("PATH=" + base_path);

        // 加载容器环境文件（对应 Bash 的 LOAD_ENV_FILE / CONTAINER_ENV_FILE）
        if (config_.load_env_file && !config_.container_env_file.empty()) {
            if (fs::exists(config_.container_env_file)) {
                std::ifstream ef(config_.container_env_file);
                if (ef.is_open()) {
                    std::string line;
                    while (std::getline(ef, line)) {
                        if (line.empty() || line[0] == '#') continue;
                        // 去除 export 前缀
                        if (line.rfind("export ", 0) == 0) {
                            line = line.substr(7);
                        }
                        if (!line.empty()) {
                            args.emplace_back(line);
                        }
                    }
                }
            }
        }

        // ── 构建最终命令字符串 ──
        // 对应 Bash 版：check_host_and_root → su -c "bash ..." 或 直接执行
        std::string prefix = get_root_prefix();
        bool use_sudo = !prefix.empty() && prefix == "sudo";

        std::string final_cmd;
        if (!prefix.empty() && !use_sudo) {
            // su -c 需要整个命令作为一个字符串参数
            std::string inner;
            for (size_t i = 0; i < args.size(); i++) {
                inner += "\"" + args[i] + "\" ";
            }
            // 添加 login shell 参数
            inner += shell;
            if (!config_.login_shell_arg.empty()) {
                inner += " " + config_.login_shell_arg;
            }
            final_cmd = prefix + " \"" + inner + "\"";
        } else {
            // 直接执行或用 sudo
            if (!prefix.empty()) {
                final_cmd = prefix + " ";
            }
            for (size_t i = 0; i < args.size(); i++) {
                final_cmd += args[i] + " ";
            }
            final_cmd += shell;
            if (!config_.login_shell_arg.empty()) {
                final_cmd += " " + config_.login_shell_arg;
            }
        }

        return final_cmd;
    }

    // ── start/stop ──

    bool ChrootRuntime::start(const Container &container, const LaunchContext *ctx) {
        Logger::step(_f("chroot.preparing", container.name()));

        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;

        // 1. 写 chroot.conf（标记启动模式，对应 Bash 版 SYSTEMD_NSPAWN 标志）
        {
            std::string conf_dir_path = rootfs + "/usr/local/etc/tmoe-linux/config";
            std::string conf_path = conf_dir_path + "/chroot.conf";
            Executor::shell("mkdir -p " + conf_dir_path);
            std::ofstream cf(conf_path);
            if (cf.is_open()) {
                cf << "CHROOT_ENABLED=true\n";
                cf << "SYSTEMD_NSPAWN=false\n";
            }
        }

        // 2. 设置 hostname（对应 Bash 版：写 /etc/hostname）
        std::string machine_name = container.name();
        std::replace(machine_name.begin(), machine_name.end(), '_', '-');
        std::string hostname_target = rootfs + "/etc/hostname"; {
            std::ofstream hf(hostname_target);
            if (hf.is_open()) {
                hf << machine_name << "\n";
            }
        }

        // 3. 移除 machine-id（让容器生成新的，对应 Bash 版）
        fs::remove(rootfs + "/etc/machine-id");
        // 对于 systemd 容器，也需要移除 /var/lib/dbus/machine-id
        fs::remove(rootfs + "/var/lib/dbus/machine-id");

        // 4. 执行挂载
        do_mounts(container);

        // 5. 修复 /dev 链接
        fix_dev_links(container);

        // 6. 生成并执行启动命令
        std::string cmd = generate_launch_cmd(container, ctx);
        Logger::step(_f("chroot.launch_cmd", cmd));

        // 注：Bash 原版在此处执行 unset LD_PRELOAD，
        //    但 C++ 每次 Executor::shell 调用独立 shell 进程，无需 unset。

        return Executor::passthrough(cmd).ok();
    }

    bool ChrootRuntime::stop(const Container &container) {
        Logger::step(_f("chroot.stopping", container.name()));

        std::string rootfs = config_.rootfs_path.empty()
                                 ? container.rootfs_path()
                                 : config_.rootfs_path;

        std::string prefix = get_root_prefix();

        // 卸载标准目录（对应 Bash 版的卸载逻辑）
        const char *dirs_to_unmount[] = {"dev/pts", "dev/shm", "dev", "proc", "sys", "etc/gitstatus", nullptr};
        for (int i = 0; dirs_to_unmount[i] != nullptr; i++) {
            std::string target = rootfs + "/" + std::string(dirs_to_unmount[i]);
            if (is_mounted(target)) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "umount -lf " + target + " 2>/dev/null || true";
                Executor::shell(cmd);
            }
        }

        // 卸载自挂载
        if (config_.mount_itself) {
            if (is_mounted(rootfs + "/")) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "umount -lf " + rootfs + "/ 2>/dev/null || true";
                Executor::shell(cmd);
            }
        }

        // 卸载 SD 卡
        if (!config_.sd_mount_point.empty()) {
            std::string target = rootfs + config_.sd_mount_point;
            if (is_mounted(target)) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "umount -lf " + target + " 2>/dev/null || true";
                Executor::shell(cmd);
            }
        }

        // 卸载 Termux 目录
        if (!config_.termux_mount_point.empty()) {
            std::string target = rootfs + config_.termux_mount_point;
            if (is_mounted(target)) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "umount -lf " + target + " 2>/dev/null || true";
                Executor::shell(cmd);
            }
        }

        // 卸载 TF 卡
        if (!config_.tf_mount_point.empty()) {
            std::string target = rootfs + config_.tf_mount_point;
            if (is_mounted(target)) {
                std::string cmd = prefix;
                if (!cmd.empty()) cmd += " ";
                cmd += "umount -lf " + target + " 2>/dev/null || true";
                Executor::shell(cmd);
            }
        }

        return true;
    }
} // namespace tmoe::domain
