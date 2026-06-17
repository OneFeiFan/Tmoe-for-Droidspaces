#include "domain/runtime.h"
#include "core/executor.h"
#include "core/logger.h"
#include <filesystem>
#include <cstdlib>

#include "container.h"

namespace fs = std::filesystem;

namespace tmoe::domain {
    // ── Bin detection ──

    std::string NspawnRuntime::detect_nspawn_bin() const {
        if (config_.nspawn_bin == "system" || config_.nspawn_bin.empty()) {
            return "systemd-nspawn";
        } else if (config_.nspawn_bin == "termux" || config_.nspawn_bin == "prefix") {
            const char *prefix = std::getenv("PREFIX");
            std::string p = prefix ? prefix : "";
            return p + "/bin/systemd-nspawn";
        }
        return config_.nspawn_bin;
    }

    // ── Args builder (对应 Bash 的 run_tmoe_qemu_command 中 nspawn 部分) ──

    void NspawnRuntime::build_nspawn_args(const Container &container, const LaunchContext *ctx,
                                          std::vector<std::string> &args) const {
        std::string rootfs = config_.rootfs_path;
        if (rootfs.empty()) {
            rootfs = container.rootfs_path();
        }

        // 1. --boot 模式
        if (config_.systemd_boot) {
            args.emplace_back("--boot");
        }

        // 2. --user (非 root)
        if (!config_.systemd_boot && config_.chroot_user != "root" && !config_.chroot_user.empty()) {
            args.emplace_back("--user");
            args.emplace_back(config_.chroot_user);
        }

        // 3. 基本参数
        args.emplace_back("--directory");
        args.emplace_back(rootfs);

        // 4. uuid
        auto r = Executor::shell("dbus-uuidgen");
        std::string uuid = r.ok() ? r.stdout_data : "";
        if (!uuid.empty()) {
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\n'), uuid.end());
            args.emplace_back("--uuid");
            args.emplace_back(uuid);
        }

        args.emplace_back("--private-users=no");
        args.emplace_back("--machine");
        args.emplace_back(container.name());

        args.emplace_back("--link-journal");
        args.emplace_back("auto");
        args.emplace_back("--resolv-conf");
        args.emplace_back("auto");
        args.emplace_back("--timezone");
        args.emplace_back("auto");
        args.emplace_back("--register");
        args.emplace_back("yes");
        args.emplace_back("--notify-ready");
        args.emplace_back("yes");

        // 5. 环境变量
        args.emplace_back("--setenv");
        args.emplace_back("TMOE_CHROOT=true");
        args.emplace_back("--setenv");
        args.emplace_back("TMOE_PROOT=false");
        args.emplace_back("--setenv");
        args.emplace_back("TMOE_DOCKER=false");
        args.emplace_back("--setenv");
        args.emplace_back("USER=" + config_.chroot_user);

        args.emplace_back("--setenv");
        args.emplace_back("PULSE_SERVER=tcp:127.0.0.1:4713");
        args.emplace_back("--setenv");
        args.emplace_back("DISPLAY=:2");
        args.emplace_back("--setenv");
        args.emplace_back("GTK_IM_MODULE=fcitx");
        args.emplace_back("--setenv");
        args.emplace_back("QT_IM_MODULE=fcitx");
        args.emplace_back("--setenv");
        args.emplace_back("XMODIFIERS=@im=fcitx");
        args.emplace_back("--setenv");
        args.emplace_back("SDL_IM_MODULE=fcitx");
        args.emplace_back("--setenv");
        args.emplace_back("CONTAINER_SYSTEMD=true");
        args.emplace_back("--setenv");
        args.emplace_back("TERM=xterm-256color");

        // LANG
        std::string locale_file = rootfs + "/usr/local/etc/tmoe-linux/locale.txt";
        if (fs::exists(locale_file)) {
            std::ifstream lf(locale_file);
            std::string lang;
            if (lf.is_open() && std::getline(lf, lang)) {
                args.emplace_back("--setenv");
                args.emplace_back("LANG=" + lang);
            } else {
                args.emplace_back("--setenv");
                args.emplace_back("LANG=C.UTF-8");
            }
        } else {
            args.emplace_back("--setenv");
            args.emplace_back("LANG=C.UTF-8");
        }

        // 加载容器环境文件
        if (config_.load_env_file && !config_.container_env_file.empty()) {
            if (fs::exists(config_.container_env_file)) {
                std::ifstream ef(config_.container_env_file);
                if (ef.is_open()) {
                    std::string line;
                    while (std::getline(ef, line)) {
                        if (line.empty() || line[0] == '#') continue;
                        if (line.rfind("export ", 0) == 0) {
                            line = line.substr(7);
                        }
                        if (line.rfind("PATH=", 0) != 0 && !line.empty()) {
                            args.emplace_back("--setenv");
                            args.emplace_back(line);
                        }
                    }
                }
            }
        }

        // PATH
        if (config_.chroot_user == "root" || config_.chroot_user.empty()) {
            args.emplace_back("--setenv");
            args.emplace_back(
                "PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games");
        } else {
            args.emplace_back("--setenv");
            args.emplace_back("PATH=/usr/local/bin:/bin:/usr/bin:/usr/games:/usr/local/games");
        }

        // 6. SD 卡挂载
        if (config_.mount_sd.empty() || config_.mount_sd == "true") {
            std::string sd_dir;
            for (const auto &d: {
                     config_.sd_dir_0, config_.sd_dir_1,
                     config_.sd_dir_2, config_.sd_dir_3, config_.sd_dir_4
                 }) {
                if (d.empty()) continue;
                if (Executor::shell("test -d " + d).ok()) {
                    sd_dir = d;
                    break;
                }
            }
            if (!sd_dir.empty()) {
                args.emplace_back("--bind");
                args.emplace_back(sd_dir + ":" + config_.sd_mount_point);
            }
        }

        // 7. Docker 目录挂载
        if (config_.mount_docker_dir) {
            if (fs::exists(config_.docker_mount_point)) {
                args.emplace_back("--bind");
                args.emplace_back(config_.docker_mount_point);
            }
        }

        // 8. GPU 设备
        if (config_.mount_dev_dri && fs::exists(config_.dev_dri)) {
            args.emplace_back("--bind");
            args.emplace_back(config_.dev_dri);
        }
        if (config_.mount_nvidia_gpu && fs::exists(config_.nvidia_gpu)) {
            args.emplace_back("--bind");
            args.emplace_back(config_.nvidia_gpu);
            if (fs::exists(config_.nvidia_ctl)) {
                args.emplace_back("--bind");
                args.emplace_back(config_.nvidia_ctl);
            }
        }

        // 9. X11 unix socket
        if (config_.mount_x11_unix && fs::exists(config_.x11_unix_dir)) {
            std::string bind_arg = config_.x11_unix_ro ? "bind-ro" : "bind";
            args.emplace_back("--" + bind_arg);
            args.emplace_back(config_.x11_unix_dir);
        }

        // 10. 自定义挂载点（最多12个）
        for (int i = 0; i < 12; i++) {
            const auto &m = config_.custom_mounts[i];
            if (!m.enabled) continue;
            if (m.source.empty() || m.dest.empty()) continue;
            if (!fs::exists(m.source)) continue;

            std::string bind_arg = m.readonly ? "bind-ro" : "bind";
            args.emplace_back("--" + bind_arg);
            args.emplace_back(m.source + ":" + m.dest);
        }
    }

    // ── 命令生成 ──

    std::string NspawnRuntime::generate_launch_cmd(const Container &container, const LaunchContext *ctx) const {
        std::vector<std::string> args;
        std::string nspawn_bin = detect_nspawn_bin();

        args.emplace_back(nspawn_bin);
        build_nspawn_args(container, ctx, args);

        // 构建完整命令字符串
        std::string cmd;
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) cmd += " ";
            cmd += args[i];
        }

        return cmd;
    }

    // ── start/stop ──

    bool NspawnRuntime::start(const Container &container, const LaunchContext *ctx) {
        Logger::step("正在准备 systemd-nspawn 容器: " + container.name());

        // 检查 systemd-nspawn 是否安装
        std::string nspawn_bin = detect_nspawn_bin();
        if (!Executor::shell("command -v " + nspawn_bin).ok()) {
            Logger::error("未找到 systemd-nspawn，请先安装 systemd-container");
            return false;
        }

        // 检查 dbus-uuidgen
        if (!Executor::shell("command -v dbus-uuidgen").ok()) {
            Logger::warn("未找到 dbus-uuidgen，将跳过 --uuid 参数");
        }

        // 修复 root 密码
        std::string rootfs = config_.rootfs_path;
        if (rootfs.empty()) {
            rootfs = container.rootfs_path();
        }
        std::string shadow_file = rootfs + "/etc/shadow";
        if (fs::exists(shadow_file)) {
            Executor::shell("sed -i -E 's/^(root):.*:/\\1::/' " + shadow_file);
        }

        // 设置 hostname
        std::string machine_name = container.name();
        std::replace(machine_name.begin(), machine_name.end(), '_', '-');
        std::ofstream hf(rootfs + "/etc/hostname");
        if (hf.is_open()) {
            hf << machine_name << "\n";
        }
        // 同时写入 /tmp 再 mv（Bash 版行为）
        std::ofstream hf_tmp("/tmp/.tmp_hostname");
        if (hf_tmp.is_open()) {
            hf_tmp << machine_name << "\n";
        }
        Executor::shell("cp /tmp/.tmp_hostname " + rootfs + "/etc/hostname");
        fs::remove("/tmp/.tmp_hostname");

        // 移除 machine-id
        fs::remove(rootfs + "/etc/machine-id");

        // 生成启动命令
        std::string cmd = generate_launch_cmd(container, ctx);
        Logger::step("Nspawn 启动命令: " + cmd);

        // 清除 LD_PRELOAD
        Executor::shell("unset LD_PRELOAD");

        // 执行：需要 root 权限
        std::string prefix;
        if (std::getenv("USER") && std::string(std::getenv("USER")) != "root") {
            if (Executor::shell("command -v sudo").ok()) {
                prefix = "sudo";
            } else {
                prefix = "su -c";
            }
        }

        if (!prefix.empty()) {
            cmd = prefix + " \"" + cmd + "\"";
        }

        return Executor::shell(cmd).ok();
    }

    bool NspawnRuntime::stop(const Container &container) {
        Logger::step("停止 systemd-nspawn 容器: " + container.name());

        // systemd-nspawn 容器可以用 machinectl 停止
        std::string cmd = "machinectl terminate " + container.name();
        if (std::getenv("USER") && std::string(std::getenv("USER")) != "root") {
            if (Executor::shell("command -v sudo").ok()) {
                cmd = "sudo " + cmd;
            }
        }

        Executor::shell(cmd);
        return true;
    }
} // namespace tmoe::domain
