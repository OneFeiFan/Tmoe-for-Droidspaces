#include "domain/runtime.h"
#include "domain/container.h"
#include "core/executor.h"
#include "core/logger.h"
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

// ── QEMU 架构检测 ──
std::string ProotRuntime::resolve_qemu_arch() const {
    const auto &c = config_;
    if (c.qemu_arch != "default") return c.qemu_arch;

    // 同一架构则无需 QEMU
    if (c.host_arch == c.container_arch) return "";

    // 容器架构 → QEMU 架构映射
    const std::unordered_map<std::string, std::string> arch_map = {
        {"amd64", "x86_64"}, {"i386", "i386"},     {"arm64", "aarch64"},
        {"armhf", "arm"},    {"armel", "armeb"},    {"mipsel", "mipsel"},
        {"mips64el", "mips64el"}, {"ppc64el", "ppc64le"}, {"s390x", "s390x"},
        {"riscv64", "riscv64"}
    };

    auto it = arch_map.find(c.container_arch);
    if (it == arch_map.end()) return "";

    std::string qemu_arch = it->second;

    // 特例：i386/armhf 在相同位宽宿主机上不需要 QEMU
    if (c.container_arch == "i386" && (c.host_arch == "amd64" || c.host_arch == "i386")) return "";
    if (c.container_arch == "armhf" && (c.host_arch == "arm64" || c.host_arch == "armhf")) return "";
    if (c.container_arch == "armel" && (c.host_arch == "arm64" || c.host_arch == "armhf" || c.host_arch == "armel")) return "";

    return qemu_arch;
}

// ── 生成启动命令 ──
std::string ProotRuntime::generate_launch_cmd(const Container &container, const LaunchContext *ctx) const {
    std::vector<std::string> args;
    apply_proot_args(container, args);

    // ── 环境变量 ──
    args.emplace_back("/usr/bin/env");
    args.emplace_back("-i");
    args.emplace_back("HOME=/root");
    args.emplace_back("TERM=xterm-256color");

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
        args.emplace_back("LANG=" + lang);
    }

    args.emplace_back("TMOE_CHROOT=true");
    args.emplace_back("TMOE_PROOT=true");

    // GUI 模式
    if (ctx && (ctx->exec_program == "startvnc" || ctx->exec_program == "novnc" ||
                ctx->exec_program_01 == "startvnc"))
        args.emplace_back("PULSE_SERVER=127.0.0.1");

    // 输入法环境变量
    args.emplace_back("SDL_IM_MODULE=fcitx");
    args.emplace_back("XMODIFIERS=@im=fcitx");
    args.emplace_back("QT_IM_MODULE=fcitx");
    args.emplace_back("GTK_IM_MODULE=fcitx");
    args.emplace_back("TMPDIR=/tmp");
    args.emplace_back("DISPLAY=:0");

    // 添加尾部命令
    std::string shell_bin = "/bin/bash";
    if (ctx && !ctx->tmoe_shell.empty()) shell_bin = ctx->tmoe_shell;

    if (ctx && (!ctx->exec_program.empty() || !ctx->exec_program_01.empty())) {
        std::string final_exec = ctx->exec_program.empty() ? ctx->exec_program_01 : ctx->exec_program;
        if (!ctx->exec_program_02.empty()) final_exec += " " + ctx->exec_program_02;
        if (!ctx->exec_program_03.empty()) final_exec += " " + ctx->exec_program_03;
        args.emplace_back(shell_bin);
        args.emplace_back("-c");
        args.emplace_back(final_exec);
    } else {
        args.emplace_back(shell_bin);
        args.emplace_back("--login");
    }

    std::string cmd;
    for (const auto &a : args) {
        if (!cmd.empty()) cmd += " ";
        cmd += a;
    }
    return cmd;
}

void ProotRuntime::apply_proot_args(const Container &container, std::vector<std::string> &args) const {
    auto c = config_;
    const std::string &rootfs = container.rootfs_path();
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string prefix = std::getenv("PREFIX") ? std::getenv("PREFIX") : "/usr";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";

    // 变量展开
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

    args.emplace_back(proot_program);

    // ── link2symlink ──
    if (c.link_to_symlink)
        args.emplace_back("--link2symlink");

    // ── root ID ──
    if (c.proot_user.empty() || c.proot_user == "root") {
        args.emplace_back("-0");
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
            args.emplace_back("-0");
        } else {
            args.emplace_back("--change-id=" + uid_str + ":" + gid_str);
        }
    }

    // ── 工作目录 ──
    std::string wd = "/root";
    if (c.work_dir != "default" && !c.work_dir.empty())
        wd = c.work_dir;
    else if (c.proot_user != "root" && !c.proot_user.empty())
        wd = "/home/" + c.proot_user;
    args.emplace_back("--cwd=" + wd);

    // ── rootfs ──
    args.emplace_back("--rootfs=" + rootfs);

    // ── QEMU 集成 ──
    std::string qemu_arch = resolve_qemu_arch();
    if (!qemu_arch.empty() && !c.skip_qemu_detection) {
        std::string qemu_path = c.qemu_32_enabled ? rv(c.qemu_user_static_32_path) : rv(c.qemu_user_static_path);
        std::string qemu_bin;
        if (c.qemu_user_bin != "default" && !c.qemu_user_bin.empty())
            qemu_bin = c.qemu_user_bin;
        else if (c.qemu_user_static)
            qemu_bin = qemu_path + "/qemu-" + qemu_arch + "-static";
        else
            qemu_bin = "qemu-" + qemu_arch;

        args.emplace_back("--qemu=" + qemu_bin);
    }

    // ── 行为标志 ──
    if (c.kill_on_exit)
        args.emplace_back("--kill-on-exit");
    if (c.proot_sysvipc)
        args.emplace_back("--sysvipc");
    if (c.proot_l)
        args.emplace_back("-L");
    if (c.proot_h)
        args.emplace_back("-H");
    if (c.proot_p)
        args.emplace_back("-p");
    if (c.fake_kernel)
        args.emplace_back("--kernel-release=" + rv(c.kernel_release + c.container_arch));

    // ── 旧 Android 兼容模式 ──
    if (c.old_android_version_compatibility_mode) {
        // 该模式下这些标志已通过 --kernel-release 等方式覆盖
    }

    // ── 挂载系统 ──
    // 基础挂载
    if (c.mount_dev) args.emplace_back("--bind=" + rv(c.dev_dir));
    if (c.mount_proc) args.emplace_back("--bind=" + rv(c.proc_dir));
    if (c.mount_sys && c.host_distro != "Android") args.emplace_back("--bind=" + rv(c.sys_dir));

    // Android 专用挂载
    if (c.host_distro == "Android") {
        if (c.mount_system) {
            std::string sysd = rv(c.system_dir);
            if (fs::exists(sysd)) args.emplace_back("--bind=" + sysd);
        }
        if (c.mount_apex) {
            std::string apexd = rv(c.apex_dir);
            if (fs::exists(apexd)) args.emplace_back("--bind=" + apexd);
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
                args.emplace_back("--bind=" + sd + ":" + rv(c.sd_mount_point));
                break;
            }
        }
    }

    // Termux 挂载
    if (c.mount_termux == "true" || c.mount_termux.empty()) {
        std::string td = rv(c.termux_dir) + "/home";
        if (fs::exists(td))
            args.emplace_back("--bind=" + td + ":" + rv(c.termux_mount_point));
    }

    // TF 卡挂载
    if (c.mount_tf == "true" || c.mount_tf.empty()) {
        std::string tfl = rv(c.tf_card_link);
        if (fs::is_symlink(tfl)) {
            auto resolved = fs::read_symlink(tfl);
            if (fs::exists(resolved))
                args.emplace_back("--bind=" + resolved.string() + ":" + rv(c.tf_mount_point));
        }
    }

    // Storage 挂载
    if (c.mount_storage == "true" || c.mount_storage.empty()) {
        std::string stor = rv(c.storage_dir);
        if (fs::exists(stor))
            args.emplace_back("--bind=" + stor + ":" + rv(c.storage_mount_point));
    }

    // Gitstatus 挂载
    if (c.mount_gitstatus && !rv(c.gitstatus_dir).empty()) {
        args.emplace_back("--bind=" + rv(c.gitstatus_dir) + ":" + rv(c.gitstatus_mount_point));
    }

    // Tmp 挂载
    if (c.mount_tmp) {
        std::string td = c.tmp_source_dir.empty() ? rv("${TMPDIR}") : rv(c.tmp_source_dir);
        args.emplace_back("--bind=" + td + ":" + rv(c.tmp_mount_point));
    }

    // Termux home 绑定
    std::string termux_home = "/data/data/com.termux/files/home";
    if (fs::exists(termux_home))
        args.emplace_back("--bind=" + termux_home + ":/host-home");

    // Dev 子挂载
    if (c.mount_dev_fd)
        args.emplace_back("--bind=/dev/fd");
    if (c.mount_dev_stdin)
        args.emplace_back("--bind=/dev/stdin");
    if (c.mount_dev_stdout)
        args.emplace_back("--bind=/dev/stdout");
    if (c.mount_dev_stderr)
        args.emplace_back("--bind=/dev/stderr");
    if (c.mount_dev_tty)
        args.emplace_back("--bind=/dev/tty");
    if (c.mount_shm_to_tmp)
        args.emplace_back("--bind=/tmp:/dev/shm");
    if (c.mount_urandom_to_random)
        args.emplace_back("--bind=/dev/urandom:/dev/random");
    if (c.mount_cap_last_cap)
        args.emplace_back("--bind=" + rv(c.cap_last_cap_source) + ":" + rv(c.cap_last_cap_mount_point));

    // 自定义挂载
    for (const auto &cm : c.custom_mounts) {
        if (cm.enabled && !cm.source.empty() && !cm.dest.empty())
            args.emplace_back("--bind=" + rv(cm.source) + ":" + rv(cm.dest));
    }

}

// ── start/stop ──
bool ProotRuntime::start(const Container &container, const LaunchContext *ctx) {
    std::string cmd = generate_launch_cmd(container, ctx);
    Logger::step("[PRoot] 启动容器: " + container.name());
    return Executor::shell(cmd).ok();
}

bool ProotRuntime::stop(const Container &container) {
    Logger::step("[PRoot] 终止容器: " + container.name());
    return Executor::shell("pkill -f 'proot.*" + container.rootfs_path() + "'").ok();
}

} // namespace tmoe::domain
