#include "domain/runtime/runtime.h"
#include "domain/runtime/container.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;
using tmoe::Executor;

namespace tmoe::domain {

// ── ProotRuntime 辅助函数 ──

static std::string resolve_var(const std::string &s, const std::string &home,
                                const std::string &tmoe_linux_dir, const std::string &prefix) {
    std::string r = s;
    // ${HOME}
    size_t pos;
    while ((pos = r.find("${HOME}")) != std::string::npos)
        r.replace(pos, 7, home);
    while ((pos = r.find("${TMOE_LINUX_DIR}")) != std::string::npos)
        r.replace(pos, 18, tmoe_linux_dir);
    while ((pos = r.find("${PREFIX}")) != std::string::npos)
        r.replace(pos, 9, prefix);
    while ((pos = r.find("${CONFIG_FOLDER}")) != std::string::npos)
        r.replace(pos, 17, tmoe_linux_dir + "/config");
    while ((pos = r.find("${TMPDIR}")) != std::string::npos) {
        const char *td = std::getenv("TMPDIR");
        r.replace(pos, 8, td ? td : "/tmp");
    }
    return r;
}

// ── 生成启动命令 ──
std::string ProotRuntime::generate_launch_cmd(const Container &container, const LaunchContext *ctx) const {
    auto c = config_;
    const std::string &rootfs = container.rootfs_path();
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/usr";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";
    auto rv = [&](const std::string &s) { return resolve_var(s, home, tmoe_linux_dir, prefix); };

    // ── 二进制选择 ──
    std::string proot_program = "proot";
    if (c.proot_bin == "termux" || c.proot_bin == "prefix")
        proot_program = prefix + "/bin/proot";
    else if (c.proot_bin == "compatibility")
        proot_program = rv(c.proot_compatible_mode_bin);
    else if (c.proot_bin == "32") {
        if (c.host_distro == "Android")
            proot_program = rv(c.proot_32_termux_bin);
    } else if (c.proot_bin != "default" && c.proot_bin != "system" && !c.proot_bin.empty())
        proot_program = c.proot_bin;

    CommandBuilder cb(proot_program);
    apply_proot_args(container, cb);

    // ── 环境变量 ──
    cb.add_arg("/usr/bin/env");
    cb.add_arg("-i");
    cb.add_arg("HOME=/root");
    cb.add_arg("TERM=xterm-256color");

    // LANG — 读取容器的 locale 配置
    {
        std::string locale_file = container.rootfs_path() + "/usr/local/etc/tmoe-linux/locale.txt";
        std::string lang = "en_US.UTF-8";
        if (fs::exists(locale_file)) {
            std::ifstream lf(locale_file);
            std::string line;
            if (lf.is_open() && std::getline(lf, line)) {
                line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                if (!line.empty()) lang = line;
            }
        }
        cb.add_arg("LANG=" + lang);
    }

    cb.add_arg("TMOE_CHROOT=true");
    cb.add_arg("TMOE_PROOT=true");

    // GUI 模式
    if (ctx && (ctx->exec_program == "startvnc" || ctx->exec_program == "novnc" ||
                ctx->exec_program_01 == "startvnc"))
        cb.add_arg("PULSE_SERVER=127.0.0.1");

    // 输入法环境变量
    cb.add_arg("SDL_IM_MODULE=fcitx");
    cb.add_arg("XMODIFIERS=@im=fcitx");
    cb.add_arg("QT_IM_MODULE=fcitx");
    cb.add_arg("GTK_IM_MODULE=fcitx");
    cb.add_arg("TMPDIR=/tmp");
    cb.add_arg("DISPLAY=:0");

    // 添加尾部命令
    std::string shell_bin = "/bin/bash";
    if (ctx && !ctx->tmoe_shell.empty()) shell_bin = ctx->tmoe_shell;

    if (ctx && (!ctx->exec_program.empty() || !ctx->exec_program_01.empty())) {
        std::string final_exec = ctx->exec_program.empty() ? ctx->exec_program_01 : ctx->exec_program;
        if (!ctx->exec_program_02.empty()) final_exec += " " + ctx->exec_program_02;
        if (!ctx->exec_program_03.empty()) final_exec += " " + ctx->exec_program_03;
        cb.add_arg(shell_bin);
        cb.add_arg("-c");
        cb.add_arg(final_exec);
    } else {
        cb.add_arg(shell_bin);
        cb.add_arg("--login");
    }

    return cb.build_string();
}

void ProotRuntime::apply_proot_args(const Container &container, CommandBuilder &cb) const {
    auto c = config_;
    const std::string &rootfs = container.rootfs_path();
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/usr";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";

    // 变量展开
    auto rv = [&](const std::string &s) { return resolve_var(s, home, tmoe_linux_dir, prefix); };

    // ── link2symlink ──
    cb.add_flag_if(c.link_to_symlink, "--link2symlink");

    // ── root ID ──
    if (c.proot_user.empty() || c.proot_user == "root") {
        cb.add_flag("-0");
    } else {
        // 从容器的 /etc/passwd 中查找 UID/GID
        std::string passwd_path = rootfs + "/etc/passwd";
        std::string uid_str, gid_str;
        // 简化版: 用 shell 读取
        std::string lookup = "grep '^" + c.proot_user + ":' " + passwd_path + " 2>/dev/null | "
                             "awk -F: '{print $3\":\"$4}'";
        auto r = Executor::shell(lookup);
        if (r.ok() && !r.stdout_data.empty()) {
            auto colon = r.stdout_data.find(':');
            if (colon != std::string::npos) {
                uid_str = r.stdout_data.substr(0, colon);
                gid_str = r.stdout_data.substr(colon + 1);
                // trim newline
                if (!gid_str.empty() && gid_str.back() == '\n') gid_str.pop_back();
            }
        }
        if (uid_str.empty() || uid_str == "0") {
            cb.add_flag("-0");
        } else {
            cb.add_kv("--change-id", uid_str + ":" + gid_str);
        }
    }

    // ── 工作目录 ──
    std::string wd = "/root";
    if (c.work_dir != "default" && !c.work_dir.empty())
        wd = c.work_dir;
    else if (c.proot_user != "root" && !c.proot_user.empty())
        wd = "/home/" + c.proot_user;
    cb.add_kv("--cwd", wd);

    // ── rootfs ──
    cb.add_kv("--rootfs", rootfs);

    // ── 行为标志 ──
    cb.add_flag_if(c.kill_on_exit, "--kill-on-exit");
    cb.add_flag_if(c.proot_sysvipc, "--sysvipc");
    cb.add_flag_if(c.proot_l, "-L");
    cb.add_flag_if(c.proot_h, "-H");
    cb.add_flag_if(c.proot_p, "-p");
    cb.add_kv_if(c.fake_kernel, "--kernel-release", rv(c.kernel_release + c.container_arch));

    // ── 旧 Android 兼容模式 ──
    if (c.old_android_version_compatibility_mode) {
        // 该模式下这些标志已通过 --kernel-release 等方式覆盖
    }

    // ── 挂载系统 ──
    // 基础挂载
    cb.add_bind_to_if(c.mount_dev, "--bind=", rv(c.dev_dir), "");
    cb.add_bind_to_if(c.mount_proc, "--bind=", rv(c.proc_dir), "");
    cb.add_bind_to_if(c.mount_sys && c.host_distro != "Android", "--bind=", rv(c.sys_dir), "");

    // Android 专用挂载
    if (c.host_distro == "Android") {
        if (c.mount_system) {
            std::string sysd = rv(c.system_dir);
            if (fs::exists(sysd)) cb.add_bind_to("--bind=", sysd, "");
        }
        if (c.mount_apex) {
            std::string apexd = rv(c.apex_dir);
            if (fs::exists(apexd)) cb.add_bind_to("--bind=", apexd, "");
        }
    }

    // SD 卡挂载（优先级机制）
    if (c.mount_sd == "true" || c.mount_sd.empty()) {
        const std::string sd_dirs[] = {
            rv(c.sd_dir_0), rv(c.sd_dir_1), rv(c.sd_dir_2),
            rv(c.sd_dir_3), rv(c.sd_dir_4), rv(c.sd_dir_5)
        };
        for (const auto &sd : sd_dirs) {
            if (!sd.empty() && fs::exists(sd)) {
                cb.add_bind_to("--bind=", sd, rv(c.sd_mount_point));
                break;
            }
        }
    }

    // Termux 挂载
    if (c.mount_termux == "true" || c.mount_termux.empty()) {
        std::string td = rv(c.termux_dir) + "/home";
        if (fs::exists(td))
            cb.add_bind_to("--bind=", td, rv(c.termux_mount_point));
    }

    // TF 卡挂载
    if (c.mount_tf == "true" || c.mount_tf.empty()) {
        std::string tfl = rv(c.tf_card_link);
        if (fs::is_symlink(tfl)) {
            auto resolved = fs::read_symlink(tfl);
            if (fs::exists(resolved))
                cb.add_bind_to("--bind=", resolved.string(), rv(c.tf_mount_point));
        }
    }

    // Storage 挂载
    if (c.mount_storage == "true" || c.mount_storage.empty()) {
        std::string stor = rv(c.storage_dir);
        if (fs::exists(stor))
            cb.add_bind_to("--bind=", stor, rv(c.storage_mount_point));
    }

    // Gitstatus 挂载
    if (c.mount_gitstatus && !rv(c.gitstatus_dir).empty()) {
        cb.add_bind_to("--bind=", rv(c.gitstatus_dir), rv(c.gitstatus_mount_point));
    }

    // Tmp 挂载
    if (c.mount_tmp) {
        std::string td = c.tmp_source_dir.empty() ? rv("${TMPDIR}") : rv(c.tmp_source_dir);
        cb.add_bind_to("--bind=", td, rv(c.tmp_mount_point));
    }

    // Termux home 绑定
    std::string termux_home = "/data/data/com.termux/files/home";
    if (fs::exists(termux_home))
        cb.add_bind_to("--bind=", termux_home, "/host-home");

    // Dev 子挂载
    if (c.mount_dev_fd)
        cb.add_bind_to("--bind=", "/dev/fd", "");
    if (c.mount_dev_stdin)
        cb.add_bind_to("--bind=", "/dev/stdin", "");
    if (c.mount_dev_stdout)
        cb.add_bind_to("--bind=", "/dev/stdout", "");
    if (c.mount_dev_stderr)
        cb.add_bind_to("--bind=", "/dev/stderr", "");
    if (c.mount_dev_tty)
        cb.add_bind_to("--bind=", "/dev/tty", "");
    if (c.mount_shm_to_tmp)
        cb.add_bind_to("--bind=", "/tmp", "/dev/shm");
    if (c.mount_urandom_to_random)
        cb.add_bind_to("--bind=", "/dev/urandom", "/dev/random");
    if (c.mount_cap_last_cap)
        cb.add_bind_to("--bind=", rv(c.cap_last_cap_source), rv(c.cap_last_cap_mount_point));

    // 自定义挂载
    for (const auto &cm : c.custom_mounts) {
        if (cm.enabled && !cm.source.empty() && !cm.dest.empty())
            cb.add_bind_to("--bind=", rv(cm.source), rv(cm.dest));
    }

}

// ── start/stop ──
bool ProotRuntime::start(const Container &container, const LaunchContext *ctx) {
    std::string cmd = generate_launch_cmd(container, ctx);
    Logger::step(_f("proot.starting", container.name()));
    return Executor::passthrough(cmd).ok();
}

bool ProotRuntime::stop(const Container &container) {
    Logger::step(_f("proot.stopping", container.name()));
    CommandBuilder cb("pkill");
    cb.add_flag("-f");
    cb.add_arg("proot.*" + container.rootfs_path());
    return Executor::shell(cb.build_string()).ok();
}

} // namespace tmoe::domain
