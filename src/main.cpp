#include "core/config.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "core/cli_parser.h"
#include "core/system_helper.h"
#include "domain/system/mirrors.h"
#include "app/manager.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/executor.h"
#include "core/str_utils.h"

/** 输出命令行使用帮助。 */
static void print_usage() {
    std::cout << _("cli.usage");
}

/** 程序入口。
 *  阶段1: 解析全局标志 (--lang, --quiet, --no-color, --help)。
 *  阶段2: 初始化多语言子系统。
 *  阶段3: 自动检测运行环境 (Termux/WSL/发行版/架构)。
 *  阶段3.1: 权限降级 — 若通过 sudo 启动，降回原始用户。
 *        真 root 环境 (login shell/容器) 不处理。
 *        需要系统权限的操作在各模块中通过命令字符串显式加 sudo。
 *  阶段3.5: 解析 CLI 位置参数，确定操作类型。
 *  阶段5: 确保工作目录存在。
 *  阶段5.1: WSL 系统初始化 (updatedb.conf + apt-blacklist + PATH)。
 *  阶段5.5: 读取持久化的 locale 偏好文件。
 *  阶段6: 构建顶层应用管理器 (18 个领域模块)。
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
    tmoe::trim_newline(lang);
    return lang;
}

/** 持久化 locale 偏好到文件。 */
static void save_locale_pref(std::string_view lang) {
    try {
        std::string home = tmoe::SystemHelper::user_home();
        std::string dir = home + "/.config/tmoe-linux";
        std::string path = dir + "/locale";
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (!ec) {
            tmoe::SystemHelper::write_file(path, std::string(lang) + "\n");
        }
    } catch (...) {
        // 某些 chroot 环境 $HOME 目录不可写，静默跳过
    }
}

int main(int argc, char *argv[]) {
    std::vector<std::string_view> pos_args;
    std::string lang = "en_US"; // 默认英文
    bool lang_from_cli = false;
    bool show_help = false;

    // 阶段1: 剥离全局标志；剩余 token → 位置参数
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            show_help = true;
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

    // --help 必须在 I18n 初始化之后，否则 _() 宏返回裸 key
    if (show_help) {
        print_usage();
        return 0;
    }

    // 阶段3: 自动检测环境
    auto cfg = tmoe::TmoeConfig::detect();

    // 阶段3.1: 权限降级 — 若通过 sudo 启动，降回原始用户
    // 真 root 环境 (login shell, 容器内) 不做处理，SUDO_USER 为空
    if (cfg.is_root) {
        const char* sudo_user = std::getenv("SUDO_USER");
        if (sudo_user && sudo_user[0] != '\0') {
            int real_uid = tmoe::platform::getuid();
            if (real_uid != 0 && tmoe::platform::seteuid(real_uid) == 0) {
                cfg.is_root = false;
                tmoe::Logger::info(_f("env.privileges_dropped", std::string(sudo_user)));
            }
        }
    }

    // 阶段3.5: 解析 CLI 确定是否需要 root 权限（按需提权）
    tmoe::LaunchContext ctx;
    if (!pos_args.empty()) {
        ctx = tmoe::CliParser::parse(pos_args);
    }

    // 阶段5: 确保工作目录存在（$HOME/.local/share/tmoe 等用户目录）
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
        // P2: WSL 下 cmd.exe 需要 Windows 系统目录在 PATH 中 (open_uri 等依赖)
        const char* existing_path = std::getenv("PATH");
        std::string new_path = "/mnt/c/WINDOWS/system32:/mnt/c/WINDOWS/system32/WindowsPowerShell/v1.0";
        if (existing_path) new_path += ":" + std::string(existing_path);
        tmoe::platform::set_env("PATH", new_path.c_str());
    }

    // 阶段5.5: 读取持久化的 locale 偏好 (优先级: CLI > 持久化 > 默认英文)
    bool first_run = false;
    if (!lang_from_cli) {
        std::string saved = load_saved_locale();
        if (!saved.empty() && saved != lang) {
            lang = saved;
            tmoe::I18n::init(lang);
        } else if (saved.empty()) {
            first_run = true;
        }
    }

    // 阶段5.6: 首次运行 — 弹出语言选择对话框 (仅交互模式)
    if (first_run && pos_args.empty()) {
        std::string choice = tmoe::Executor::tui_select(
            cfg.tui_bin + " --title \"tmoe-linux\" "
            "--menu \"Language / 语言\\n\\nPlease select your preferred language.\" 12 50 2 "
            "\"zh_CN\" \"中文 (简体)  /  Chinese (Simplified)\" "
            "\"en_US\" \"English\"");
        if (!choice.empty()) {
            lang = choice;
            tmoe::I18n::init(lang);
            save_locale_pref(lang);
            cfg.locale = (lang == "zh_CN") ? "zh_CN.UTF-8" : "en_US.UTF-8";
        }
    }

    // 阶段5.7: 首次运行 — 镜像源自动测速选择 (仅交互模式)
    // SWITCH_MIRROR=true 的发行版: Debian/Ubuntu/Kali/Alpine (非 Deepin)
    if (first_run && pos_args.empty()) {
        std::string distro = cfg.linux_distro;
        bool should_switch = (distro == "debian" || distro == "alpine") &&
                             cfg.sub_distro != "deepin";
        if (should_switch) {
            std::string yes_label = _("mirror.auto_yes");
            std::string no_label = _("mirror.auto_skip");
            std::string cmd = cfg.tui_bin +
                " --title \"tmoe-linux\""
                " --yes-button \"" + yes_label + "\""
                " --no-button \"" + no_label + "\""
                " --yesno \"" + std::string(_("mirror.first_run_prompt")) + "\" 0 0";
            if (tmoe::Executor::passthrough(cmd).exit_code == 0) {
                tmoe::domain::MirrorManager mirror_mgr(cfg);
                mirror_mgr.auto_select();
            }
        }
    }

    // 阶段6: 初始化顶层应用管理器 (传入 save_locale_pref 回调)
    tmoe::app::Manager manager(std::move(cfg), save_locale_pref);

    // 阶段7: 路由至交互模式或命令分发
    if (pos_args.empty()) {
        // TMOE_UI_MODE 环境变量控制 UI 模式：
        //   未设置或 "legacy" — 旧版字符串拼接路由
        //   "new"   — MenuEngine + DelegateAction 桥接（与旧版功能等价）
        //   "plugin" — MenuRegistry 驱动的纯插件化 UI
        return manager.run_interactive_plugin();
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
