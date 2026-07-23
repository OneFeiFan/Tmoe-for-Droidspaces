#include "domain/runtime/runtime.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/str_utils.h"
#include "domain/system/package_manager.h"
#include <filesystem>
#include <cstdlib>

#include "domain/runtime/container.h"
#include "core/i18n.h"

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

    // ── Args builder ──

    void NspawnRuntime::build_nspawn_args(const Container &container, const LaunchContext *ctx,
                                          CommandBuilder &cb) const {
        std::string rootfs = config_.rootfs_path;
        if (rootfs.empty()) {
            rootfs = container.rootfs_path();
        }

        // 1. --boot 模式
        if (config_.systemd_boot) {
            cb.add_flag("--boot");
        }

        // 2. --user (非 root)
        if (!config_.systemd_boot && config_.chroot_user != "root" && !config_.chroot_user.empty()) {
            cb.add_opt("--user", config_.chroot_user);
        }

        // 3. 基本参数
        cb.add_opt("--directory", rootfs);

        // 4. uuid
        auto r = Executor::shell("dbus-uuidgen");
        std::string uuid = r.ok() ? r.stdout_data : "";
        if (!uuid.empty()) {
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\n'), uuid.end());
            cb.add_opt("--uuid", uuid);
        }

        cb.add_flag("--private-users=no");
        cb.add_opt("--machine", container.name());

        cb.add_opt("--link-journal", "auto");
        cb.add_opt("--resolv-conf", "auto");
        cb.add_opt("--timezone", "auto");
        cb.add_opt("--register", "yes");
        cb.add_opt("--notify-ready", "yes");

        // 5. 环境变量
        cb.add_opt("--setenv", "TMOE_CHROOT=true");
        cb.add_opt("--setenv", "TMOE_PROOT=false");
        cb.add_opt("--setenv", "TMOE_DOCKER=false");
        cb.add_opt("--setenv", "USER=" + config_.chroot_user);

        cb.add_opt("--setenv", "PULSE_SERVER=tcp:127.0.0.1:4713");
        cb.add_opt("--setenv", "DISPLAY=:2");
        cb.add_opt("--setenv", "GTK_IM_MODULE=fcitx");
        cb.add_opt("--setenv", "QT_IM_MODULE=fcitx");
        cb.add_opt("--setenv", "XMODIFIERS=@im=fcitx");
        cb.add_opt("--setenv", "SDL_IM_MODULE=fcitx");
        cb.add_opt("--setenv", "CONTAINER_SYSTEMD=true");
        cb.add_opt("--setenv", "TERM=xterm-256color");

        // LANG
        std::string locale_file = rootfs + "/usr/local/etc/tmoe-linux/locale.txt";
        if (fs::exists(locale_file)) {
            std::ifstream lf(locale_file);
            std::string lang;
            if (lf.is_open() && std::getline(lf, lang)) {
                cb.add_opt("--setenv", "LANG=" + lang);
            } else {
                cb.add_opt("--setenv", "LANG=C.UTF-8");
            }
        } else {
            cb.add_opt("--setenv", "LANG=C.UTF-8");
        }

        // 加载容器环境文件
        if (config_.load_env_file && !config_.container_env_file.empty()) {
            if (fs::exists(config_.container_env_file)) {
                std::ifstream ef(config_.container_env_file);
                if (ef.is_open()) {
                    std::string line;
                    while (std::getline(ef, line)) {
                        if (line.empty() || line[0] == '#') continue;
                        if (starts_with(line, "export ")) {
                            line = line.substr(7);
                        }
                        if (!starts_with(line, "PATH=") && !line.empty()) {
                            cb.add_opt("--setenv", line);
                        }
                    }
                }
            }
        }

        // PATH（含 CONTAINER_BIN_PATH 前缀）
        std::string container_bin_path;
        if (config_.load_env_file && !config_.container_env_file.empty() && fs::exists(config_.container_env_file)) {
            std::ifstream ef(config_.container_env_file);
            std::string line;
            while (std::getline(ef, line)) {
                if (starts_with(line, "CONTAINER_BIN_PATH=")) {
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
        std::string full_path = container_bin_path.empty() ? base_path : container_bin_path + ":" + base_path;
        cb.add_opt("--setenv", "PATH=" + full_path);

        // 6. SD 卡挂载
        if (config_.mount_sd.empty() || config_.mount_sd == "true") {
            std::string sd_dir;
            for (const auto &d: {
                    config_.sd_dir_0, config_.sd_dir_1,
                    config_.sd_dir_2, config_.sd_dir_3, config_.sd_dir_4, config_.sd_dir_5
            }) {
                if (d.empty()) continue;
                if (CommandBuilder("test").add_flag("-d").add_arg(d).execute().ok()) {
                    sd_dir = d;
                    break;
                }
            }
            if (!sd_dir.empty()) {
                cb.add_opt("--bind", sd_dir + ":" + config_.sd_mount_point);
            }
        }

        // 7. Docker 目录挂载
        if (config_.mount_docker_dir) {
            if (fs::exists(config_.docker_mount_point)) {
                cb.add_opt("--bind", config_.docker_mount_point);
            }
        }

        // 8. GPU 设备
        if (config_.mount_dev_dri && fs::exists(config_.dev_dri)) {
            cb.add_opt("--bind", config_.dev_dri);
        }
        if (config_.mount_nvidia_gpu && fs::exists(config_.nvidia_gpu)) {
            cb.add_opt("--bind", config_.nvidia_gpu);
            if (fs::exists(config_.nvidia_ctl)) {
                cb.add_opt("--bind", config_.nvidia_ctl);
            }
        }

        // 9. X11 unix socket
        if (config_.mount_x11_unix && fs::exists(config_.x11_unix_dir)) {
            std::string bind_arg = config_.x11_unix_ro ? "bind-ro" : "bind";
            cb.add_opt("--" + bind_arg, config_.x11_unix_dir);
        }

        // 10. 自定义挂载点（最多12个）
        for (int i = 0; i < 12; i++) {
            const auto &m = config_.custom_mounts[i];
            if (!m.enabled) continue;
            if (m.source.empty() || m.dest.empty()) continue;
            if (!fs::exists(m.source)) continue;

            std::string bind_arg = m.readonly ? "bind-ro" : "bind";
            cb.add_opt("--" + bind_arg, m.source + ":" + m.dest);
        }
    }

    // ── 命令生成 ──

    std::string NspawnRuntime::generate_launch_cmd(const Container &container, const LaunchContext *ctx) const {
        std::string nspawn_bin = detect_nspawn_bin();
        CommandBuilder cb(nspawn_bin);
        build_nspawn_args(container, ctx, cb);
        return cb.build_string();
    }

    // ── start/stop ──

    bool NspawnRuntime::start(const Container &container, const LaunchContext *ctx) {
        Logger::step(_f("nspawn.preparing", container.name()));

        // 检查 systemd-nspawn 是否安装
        std::string nspawn_bin = detect_nspawn_bin();
        if (!PackageManager::is_command_available(nspawn_bin)) {
            Logger::error(_("nspawn.not_found"));
            return false;
        }

        // 检查 dbus-uuidgen
        if (!PackageManager::is_command_available("dbus-uuidgen")) {
            Logger::warn(_("nspawn.no_dbus_uuid"));
        }

        // 修复 root 密码
        std::string rootfs = config_.rootfs_path;
        if (rootfs.empty()) {
            rootfs = container.rootfs_path();
        }
        std::string shadow_file = rootfs + "/etc/shadow";
        if (fs::exists(shadow_file)) {
            CommandBuilder("sed").add_flag("-i").add_flag("-E")
                    .add_arg("s/^(root):.*:/\\1::/").add_arg(shadow_file).execute();
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
        CommandBuilder("cp").add_arg("/tmp/.tmp_hostname")
                .add_arg(rootfs + "/etc/hostname").execute();
        fs::remove("/tmp/.tmp_hostname");

        // 移除 machine-id
        fs::remove(rootfs + "/etc/machine-id");

        // 持久化启动脚本（对应 Bash 版的 TMP_FILE heredoc + 复制/移动）
        {
            std::string startup_dir = rootfs + "/usr/local/etc/tmoe-linux/container";
            std::string startup_script = startup_dir + "/nspawn_startup.sh";
            std::string tmp_file = "/tmp/.TMOE_NSPAWN_STARTUP";
            CommandBuilder("mkdir").add_flag("-p").add_arg(startup_dir).execute();
            // 生成启动脚本（简化版，完整版包含所有配置变量）
            std::ofstream sf(tmp_file);
            if (sf.is_open()) {
                sf << "#!/usr/bin/env bash\n";
                sf << "# systemd-nspawn startup script generated by tmoe-cpp\n";
                sf << "NSPAWN_BIN=" << detect_nspawn_bin() << "\n";
                sf << "ROOTFS_DIR=" << rootfs << "\n";
                sf << "CONTAINER_NAME=" << container.name() << "\n";
                sf << "exec ${NSPAWN_BIN} --directory ${ROOTFS_DIR} --machine ${CONTAINER_NAME}\n";
            }
            Executor::shell("cp " + tmp_file + " " + startup_script + " && chmod a+rx " + startup_script);
            CommandBuilder("mv").add_flag("-f").add_arg(tmp_file)
                    .add_arg(startup_dir + "/nspawn").execute();
        }

        // 生成启动命令
        std::string cmd = generate_launch_cmd(container, ctx);
        Logger::step(_f("nspawn.launch_cmd", cmd));

        // 注：Bash 原版在此处执行 unset LD_PRELOAD，
        //    但 C++ 每次 Executor::shell 调用独立 shell 进程，无需 unset。
        //   如需清除环境变量，请使用 C++ unsetenv("LD_PRELOAD")。

        // 执行：需要 root 权限
        std::string prefix;
        if (std::getenv("USER") && std::string(std::getenv("USER")) != "root") {
            if (PackageManager::is_command_available("sudo")) {
                prefix = "sudo";
            } else {
                prefix = "su -c";
            }
        }

        if (!prefix.empty()) {
            cmd = prefix + " \"" + cmd + "\"";
        }

        return Executor::passthrough(cmd).ok();
    }

    bool NspawnRuntime::stop(const Container &container) {
        Logger::step(_f("nspawn.stopping", container.name()));

        // systemd-nspawn 容器可以用 machinectl 停止
        CommandBuilder cb("machinectl");
        cb.add_arg("terminate").add_arg(container.name());
        if (std::getenv("USER") && std::string(std::getenv("USER")) != "root") {
            if (PackageManager::is_command_available("sudo")) {
                cb.set_prefix("sudo");
            }
        }

        Executor::passthrough(cb.build_string());
        return true;
    }
} // namespace tmoe::domain
