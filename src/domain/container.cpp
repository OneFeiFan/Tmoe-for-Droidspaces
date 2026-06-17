#include "domain/container.h"

namespace tmoe::domain {
    Container::Container(std::string name, std::string distro, std::string version,
                         std::string rootfs_path, ContainerMode mode, const TmoeConfig &cfg)
        : name_(std::move(name)),
          distro_(std::move(distro)),
          version_(std::move(version)),
          rootfs_path_(std::move(rootfs_path)),
          mode_(mode),
          cfg_(cfg) {
        // 根据模式注入运行时策略
        if (mode_ == ContainerMode::Proot) {
            runtime_ = std::make_unique<ProotRuntime>();
        } else {
            // TODO: 实现 ChrootRuntime 和 NspawnRuntime
            throw std::invalid_argument("目前仅支持引导 PRoot 运行时");
        }
    }

    bool Container::start(const LaunchContext *ctx) const {
        if (!runtime_) return false;
        return runtime_->start(*this, ctx);
    }

    bool Container::stop() const {
        if (!runtime_) return false;
        return runtime_->stop(*this);
    }

    bool Container::install() const {
        // 使用压缩包策略（避免需要原生 root 权限）
        RootfsTarballInstaller installer;
        return installer.install(*this);
    }

    // ── ProotRuntime 实现 ──

    std::string ProotRuntime::generate_launch_cmd(const Container &container, const LaunchContext *ctx) const {
        const auto &cfg = container.get_cfg();
        auto builder = CommandBuilder("proot");

        builder.add_arg_if(cfg.is_termux, "--link2symlink")
                .add_env("PROOT_NO_SECCOMP", "1")
                .add_arg("-0")
                .add_arg("-r").add_arg(container.rootfs_path())
                .add_arg("-w").add_arg("/root");

        builder.add_bind("/dev").add_bind("/sys").add_bind("/proc")
                .add_bind_if(cfg.is_termux, "/data/data/com.termux/files/home", "/host-home")
                .add_bind_if(!cfg.backup_dir.empty(), cfg.backup_dir.string(), "/sdcard-backup");

        builder.add_arg("/usr/bin/env")
                .add_arg("-i")
                .add_arg("HOME=/root")
                .add_arg("TERM=xterm-256color")
                .add_arg("LANG=" + cfg.locale);

        // GUI 模式激活时注入 PulseAudio 服务环境变量
        if (ctx && (ctx->exec_program == "startvnc" || ctx->exec_program == "novnc" || ctx->exec_program_01 ==
                    "startvnc")) {
            builder.add_arg("PULSE_SERVER=127.0.0.1");
        }

        // 根据 LaunchContext 构建尾部命令
        std::string shell_bin = "/bin/bash";
        if (ctx && !ctx->tmoe_shell.empty()) {
            shell_bin = ctx->tmoe_shell;
        }

        if (ctx && (!ctx->exec_program.empty() || !ctx->exec_program_01.empty())) {
            std::string final_exec = ctx->exec_program.empty() ? ctx->exec_program_01 : ctx->exec_program;

            if (!ctx->exec_program_02.empty()) final_exec += " " + ctx->exec_program_02;
            if (!ctx->exec_program_03.empty()) final_exec += " " + ctx->exec_program_03;

            builder.add_arg(shell_bin).add_arg("-c").add_arg(final_exec);
        } else {
            // 默认: 交互式登录 shell
            builder.add_arg(shell_bin).add_arg("--login");
        }

        return builder.build_string();
    }

    bool ProotRuntime::start(const Container &container, const LaunchContext *ctx) {
        // 启动前: 将宿主机脚本注入容器
        if (ctx) {
            std::string container_tmp_dir = container.rootfs_path() + "/usr/local/etc/tmoe-linux/environment/temporary";

            auto inject_script = [&](const std::string &script_path) {
                if (!script_path.empty() && fs::exists(script_path)) {
                    Logger::step("检测到多段投递指令，正在将宿主机脚本注入容器: " + script_path);
                    Executor::shell("mkdir -p " + container_tmp_dir);
                    Executor::shell("cp -rf " + script_path + " " + container_tmp_dir + "/");

                    std::string filename = fs::path(script_path).filename().string();
                    Executor::shell("chmod a+rx " + container_tmp_dir + "/" + filename);
                }
            };

            inject_script(ctx->temporary_script_file_01);
            inject_script(ctx->temporary_script_file_02);
            inject_script(ctx->temporary_script_file_03);
        }

        std::string cmd = generate_launch_cmd(container, ctx);
        Logger::step("正在引导启动 PRoot 容器: " + container.name());

        return Executor::shell(cmd).ok();
    }

    bool ProotRuntime::stop(const Container &container) {
        Logger::step("终止 PRoot 容器运行: " + container.name());
        return Executor::shell("pkill -f 'proot -r " + container.rootfs_path() + "'").ok();
    }
} // namespace tmoe::domain
