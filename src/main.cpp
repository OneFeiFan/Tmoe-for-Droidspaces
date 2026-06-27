#include "core/config.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "core/cli_parser.h"
#include "core/system_helper.h"
#include "app/manager.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/executor.h"

/** 输出命令行使用帮助。 */
static void print_usage() {
    std::cout << _("cli.usage");
}

/** 程序入口。
 *  阶段1: 解析全局标志 (--lang, --quiet, --no-color, --help)。
 *  阶段2: 初始化多语言子系统。
 *  阶段3: 自动检测运行环境。
 *  阶段3.5: 解析 CLI，确定操作类型。
 *  阶段4: 按需提权 — 仅对需要 root 的操作（chroot/安装/镜像源）提权；
 *         VNC 启动/停止等日常操作以普通用户身份运行。
 *  阶段5: 确保工作目录存在。
 *  阶段6: 构建顶层应用管理器。
 *  阶段7: 路由至交互式 TUI 或命令行分发模式。
 */
/** 读取持久化的 locale 偏好 (若存在)。 */
static std::string load_saved_locale() {
    std::string home = tmoe::SystemHelper::user_home();
    std::string path = home + "/.config/tmoe-linux/locale";
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string lang;
    std::getline(f, lang);
    // 清理换行符
    while (!lang.empty() && (lang.back() == '\n' || lang.back() == '\r'))
        lang.pop_back();
    return lang;
}

/** 持久化 locale 偏好到文件。 */
static void save_locale_pref(std::string_view lang) {
    std::string home = tmoe::SystemHelper::user_home();
    std::string dir = home + "/.config/tmoe-linux";
    std::string path = dir + "/locale";
    fs::create_directories(dir);
    std::ofstream f(path);
    if (f.is_open()) f << lang << "\n";
}

int main(int argc, char *argv[]) {
    std::vector<std::string_view> pos_args;
    std::string lang = "en_US"; // 默认英文
    bool lang_from_cli = false;

    // 阶段1: 剥离全局标志；剩余 token → 位置参数
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--lang" && i + 1 < argc) {
            lang = argv[++i];
            lang_from_cli = true;
        } else if (arg == "--quiet") {
            tmoe::Logger::quiet_mode = true;
        } else if (arg == "--no-color") {
            tmoe::Logger::enable_color = false;
        } else {
            pos_args.push_back(arg);
        }
    }

    // 阶段2: 加载 i18n 翻译 (先用默认/CLI 值)
    tmoe::I18n::init(lang);

    // 阶段3: 自动检测环境
    auto cfg = tmoe::TmoeConfig::detect();

    // 阶段3.5: 解析 CLI 确定是否需要 root 权限（按需提权）
    tmoe::LaunchContext ctx;
    if (!pos_args.empty()) {
        ctx = tmoe::CliParser::parse(pos_args);
    }

    // 阶段5: 确保工作目录存在（非 root 时静默跳过系统级目录）
    cfg.ensure_dirs();

    // 阶段5.1: WSL 系统初始化（通过 sudo 写系统配置文件）
    if (cfg.is_wsl) {
        if (!fs::exists("/etc/updatedb.conf")) {
            tmoe::Executor::shell(
                "sudo tee /etc/updatedb.conf >/dev/null <<'TMOE_EOF'\n"
                "PRUNE_BIND_MOUNTS=\"yes\"\n"
                "PRUNENAMES=\".git .bzr .hg .svn\"\n"
                "PRUNEPATHS=\"/tmp /var/spool /media /var/lib/os-prober /var/lib/ceph "
                "/home/.ecryptfs /var/lib/schroot /mnt\"\n"
                "PRUNEFS=\"NFS nfs nfs4 rpc_pipefs afs binfmt_misc proc smbfs autofs "
                "iso9660 ncpfs coda devpts ftpfs devfs devtmpfs fuse.mfs shm sysfs "
                "cifs rmpfs cgroup fuse.sshfs curlftpfs ceph fuse.ceph fuse.glusterfs "
                "fuse.bpf fuse.rclone configfs ecryptfs\"\n"
                "TMOE_EOF");
        }
        if (!fs::exists("/etc/apt/preferences.d/tmoe-wsl-blacklist")) {
            tmoe::Executor::shell(
                "sudo mkdir -p /etc/apt/preferences.d && "
                "sudo tee /etc/apt/preferences.d/tmoe-wsl-blacklist >/dev/null <<'TMOE_EOF'\n"
                "# tmoe-linux WSL package blacklist\n"
                "Package: acpid acpi-support modemmanager\n"
                "Pin: release *\n"
                "Pin-Priority: -1\n"
                "TMOE_EOF");
        }
    }

    // 阶段5.5: 读取持久化的 locale 偏好 (优先级: CLI > 持久化 > 默认英文)
    if (!lang_from_cli) {
        std::string saved = load_saved_locale();
        if (!saved.empty() && saved != lang) {
            lang = saved;
            tmoe::I18n::init(lang);
        }
    }

    // 阶段6: 初始化顶层应用管理器 (传入 save_locale_pref 回调)
    tmoe::app::Manager manager(std::move(cfg), save_locale_pref);

    // 阶段7: 路由至交互模式或命令分发
    if (pos_args.empty()) {
        return manager.run_interactive();
    }

    // 复刻原版 Bash 行为：位置参数过多时发出警告
    if (pos_args.size() >= 7) {
        tmoe::Logger::error(_("cli.too_many_args"));
        auto retype_hint = _("cli.retype_hint");
        tmoe::Logger::info(retype_hint + " tmoe "
                          + std::string(pos_args[0]) + " "
                          + std::string(pos_args[1]) + " "
                          + std::string(pos_args[2]) + " "
                          + std::string(pos_args[3]) + " "
                          + std::string(pos_args[4]) + " "
                          + std::string(pos_args[5]));
    }

    return manager.run_launch_context(ctx);
}
