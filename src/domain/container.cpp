#include "domain/container.h"
#include "core/logger.h"
#include "core/executor.h"
#include <stdexcept>

#include "installer.hpp"
#include "core/command_builder.hpp"

namespace tmoe::domain {
    // ── 1. Container 实体的实现 ──
    Container::Container(std::string name, std::string distro, std::string version,
                         std::string rootfs_path, ContainerMode mode, const TmoeConfig &cfg)
        : name_(std::move(name)),
          distro_(std::move(distro)),
          version_(std::move(version)),
          rootfs_path_(std::move(rootfs_path)),
          mode_(mode),
          cfg_(cfg) {
        // <-- 【关键修复】必须在初始化列表中绑定引用

        // 动态注入运行时策略
        if (mode_ == ContainerMode::Proot) {
            runtime_ = std::make_unique<ProotRuntime>();
        } else {
            // 等待 Chroot/Nspawn 策略实现后再放开
            throw std::invalid_argument("目前仅支持引导 PRoot 运行时");
        }
    }

    bool Container::start(const LaunchContext* ctx) const {
        if (!runtime_) return false;
        return runtime_->start(*this, ctx); // 透传上下文
    }

    bool Container::stop() const {
        if (!runtime_) return false;
        return runtime_->stop(*this);
    }

    bool Container::install() const {
        // 判定当前宿主环境（真实项目中从 TmoeConfig 传入）

        // 使用“压缩包直连策略”进行安装，完全绕过需要原生 root 权限的复杂步骤
        RootfsTarballInstaller installer;
        return installer.install(*this);
    }

    // ── 2. ProotRuntime 策略的实现 ──
    std::string ProotRuntime::generate_launch_cmd(const Container &container, const LaunchContext* ctx) const {
        const auto &cfg = container.get_cfg();
        auto builder = CommandBuilder("proot");

        // ... (保留原本 builder.add_arg_if, add_bind 等基础挂载参数) ...
        builder.add_arg_if(cfg.is_termux, "--link2symlink")
               .add_env("PROOT_NO_SECCOMP", "1")
               .add_arg("-0")
               .add_arg("-r").add_arg(container.rootfs_path())
               .add_arg("-w").add_arg("/root");

        // ... (保留 dev/sys/proc 等挂载) ...
        builder.add_bind("/dev").add_bind("/sys").add_bind("/proc")
               .add_bind_if(cfg.is_termux, "/data/data/com.termux/files/home", "/host-home")
               .add_bind_if(!cfg.backup_dir.empty(), cfg.backup_dir.string(), "/sdcard-backup");

        builder.add_arg("/usr/bin/env")
               .add_arg("-i")
               .add_arg("HOME=/root")
               .add_arg("TERM=xterm-256color")
               .add_arg("LANG=" + cfg.locale);

        // 新增：识别是否启动了 GUI 服务，如果是，注入声音路由环境变量
        if (ctx && (ctx->exec_program == "startvnc" || ctx->exec_program == "novnc" || ctx->exec_program_01 == "startvnc")) {
            builder.add_arg("PULSE_SERVER=127.0.0.1");
        }

        // ── 核心修复：根据 LaunchContext 动态生成尾部执行命令 ──
        std::string shell_bin = "/bin/bash";
        if (ctx && !ctx->tmoe_shell.empty()) {
            shell_bin = ctx->tmoe_shell; // 支持用户通过 $3 或 $4 强制指定 ash/zsh 等
        }

        if (ctx && (!ctx->exec_program.empty() || !ctx->exec_program_01.empty())) {
            // 如果用户指定了快捷程序 (如 startvnc, novnc) 或者后续附加了执行命令
            std::string final_exec = ctx->exec_program.empty() ? ctx->exec_program_01 : ctx->exec_program;

            // 将后续的多段命令拼接起来
            if (!ctx->exec_program_02.empty()) final_exec += " " + ctx->exec_program_02;
            if (!ctx->exec_program_03.empty()) final_exec += " " + ctx->exec_program_03;

            // 使用 shell -c 来包裹执行这些动态指令
            builder.add_arg(shell_bin).add_arg("-c").add_arg(final_exec);
        } else {
            // 默认情况：交互式登录
            builder.add_arg(shell_bin).add_arg("--login");
        }

        return builder.build_string();
    }

    bool ProotRuntime::start(const Container &container, const LaunchContext* ctx) {
        // ── 核心修复：Pre-Launch 脚本注入拦截 ──
        if (ctx) {
            // 对齐原版逻辑：设立容器内的临时启动目录
            std::string container_tmp_dir = container.rootfs_path() + "/usr/local/etc/tmoe-linux/environment/temporary";

            auto inject_script = [&](const std::string& script_path) {
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
