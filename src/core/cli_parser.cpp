#include "core/cli_parser.h"

namespace tmoe {

bool CliParser::is_path_argument(std::string_view arg) {
    if (arg.empty()) return false;

    std::filesystem::path p(arg);
    if (p.is_absolute()) return true;

    // 检测相对路径前缀（跨平台，支持正反斜杠）
    if (arg.rfind("./", 0) == 0 || arg.rfind("../", 0) == 0 ||
        arg.rfind(".\\", 0) == 0 || arg.rfind("..\\", 0) == 0) {
        return true;
    }

    return false;
}

/** 通过六阶段状态机解析位置参数，对齐原版 Bash 逻辑：
 *  $1 → 运行模式, $2 → 发行版, $3 → 版本/架构, $4/$5/$6 → 执行程序/脚本/软链接。
 */
LaunchContext CliParser::parse(const std::vector<std::string_view>& pos_args) {
    LaunchContext ctx;
    if (pos_args.empty()) {
        ctx.mode = LaunchMode::Interactive;
        return ctx;
    }

    // 还原原始参数字符串（等价于 Bash $*）
    std::ostringstream oss;
    for (size_t i = 0; i < pos_args.size(); ++i) {
        oss << pos_args[i] << (i + 1 < pos_args.size() ? " " : "");
    }
    ctx.raw_parameters = oss.str();

    // 阶段1: 解析 $1 — 核心路由状态机
    std::string_view arg1 = pos_args[0];
    // needs_root 默认为 true（安全优先），以下白名单显式关闭：
    //   proot、ls、zsh、theme、aria2、Help、
    //   --start-vnc/--stop-vnc/--start-novnc/--start-xsdl/--start-x11vnc、
    //   --vncpasswd/--choose-vnc-port、
    //   -novnc/-vnc/-stop 快捷方式

    if (arg1.rfind("p", 0) == 0 || arg1 == "proot") {
        ctx.mode = LaunchMode::Proot;
        ctx.needs_root = false;  // proot 非特权
    } else if (arg1.rfind("c", 0) == 0 || arg1 == "chroot") {
        ctx.mode = LaunchMode::Chroot;
    } else if (arg1 == "systemd" || arg1 == "sd" || arg1 == "systemctl" || arg1 == "ns" || arg1 == "nspawn") {
        ctx.mode = LaunchMode::Nspawn;
    } else if (arg1 == "ls") {
        ctx.mode = LaunchMode::ListContainers;
        ctx.needs_root = false;
        return ctx;
    } else if (arg1 == "zsh") {
        ctx.mode = LaunchMode::ZshManager;
        ctx.needs_root = false;
        return ctx;
    } else if (arg1 == "theme") {
        ctx.mode = LaunchMode::ThemeManager;
        ctx.needs_root = false;
        return ctx;
    } else if (arg1 == "aria2") {
        ctx.mode = LaunchMode::Aria2Manager;
        ctx.needs_root = false;
        return ctx;
    } else if (arg1 == "i" || arg1 == "-i") {
        ctx.mode = LaunchMode::DebianInstall;
        return ctx;
    } else if (arg1 == "m" || arg1 == "manager") {
        ctx.mode = LaunchMode::ManagerMenu;
        return ctx;
    } else if (arg1 == "-m" || arg1 == "mirror" || arg1 == "-mirror" || arg1 == "-tuna") {
        ctx.mode = LaunchMode::MirrorManager;
        return ctx;
    } else if (arg1 == "t" || arg1 == "tool") {
        ctx.mode = LaunchMode::ToolMenu;
        return ctx;
    } else if (arg1.rfind("--auto-install-gui-", 0) == 0 ||
               arg1 == "--install-gui" || arg1 == "install-gui" ||
               arg1 == "-b" || arg1 == "-c" || arg1 == "-x" ||
               arg1 == "--vncpasswd" || arg1 == "--choose-vnc-port" ||
               arg1 == "--fix-dbus" ||
               arg1 == "--start-vnc" || arg1 == "--stop-vnc" ||
               arg1 == "--start-xsdl" || arg1 == "--start-x11vnc" ||
               arg1 == "--start-novnc" || arg1 == "--auto-install-vscode" ||
               arg1 == "gui") {
        ctx.mode = LaunchMode::GuiManager;
        // ./tmoes gui --start-novnc → gui_flag 应为 "--start-novnc"
        if (arg1 == "gui" && pos_args.size() > 1) {
            std::string_view second = pos_args[1];
            if (!second.empty() && (second.rfind("--", 0) == 0 || second[0] == '-')) {
                ctx.gui_flag = second;
            } else {
                ctx.gui_flag = arg1;
            }
        } else {
            ctx.gui_flag = arg1;
        }

        // 白名单: VNC 启动/停止/密码/端口操作无需 root
        if (ctx.gui_flag == "--start-vnc" || ctx.gui_flag == "--stop-vnc" ||
            ctx.gui_flag == "--start-novnc" || ctx.gui_flag == "--start-xsdl" ||
            ctx.gui_flag == "--start-x11vnc" ||
            ctx.gui_flag == "--vncpasswd" || ctx.gui_flag == "--choose-vnc-port") {
            ctx.needs_root = false;
        }
        // 其他 GUI 操作（安装、配置、交互菜单）保持默认 true
        return ctx;
    } else {
        ctx.mode = LaunchMode::Help;
        ctx.needs_root = false;
        return ctx;
    }

    if (pos_args.size() > 0) {
        std::string_view first = pos_args[0];

        // 直接快捷方式 (如 -novnc, -vnc, -stop) — 覆写模式为 Proot
        if (first == "-novnc" || first == "-n" || (first == "m" && pos_args.size() > 1 && pos_args[1] == "-n")) {
            ctx.mode = LaunchMode::Proot;
            ctx.exec_program = "novnc";
            ctx.needs_root = false;
            return ctx;
        }
        if (first == "-v" || first == "-vnc") {
            ctx.mode = LaunchMode::Proot;
            ctx.exec_program = "startvnc";
            ctx.needs_root = false;
            return ctx;
        }
        if (first == "-s" || first == "-stop") {
            ctx.mode = LaunchMode::Proot;
            ctx.exec_program = "stopvnc";
            ctx.needs_root = false;
            return ctx;
        }
    }

    // 阶段2: 解析 $2 — 发行版名称匹配
    if (pos_args.size() > 1) {
        std::string_view arg2 = pos_args[1];
        if (arg2 == "a" || arg2 == "arch") ctx.distro_name = "arch";
        else if (arg2 == "al" || arg2 == "ap" || arg2 == "alpine") ctx.distro_name = "alpine";
        else if (arg2 == "arm" || arg2 == "armbian") ctx.distro_name = "armbian";
        else if (arg2 == "c" || arg2 == "ce" || arg2 == "cent" || arg2 == "centos") ctx.distro_name = "centos";
        else if (arg2 == "d" || arg2 == "deb" || arg2 == "debian") ctx.distro_name = "debian";
        else if (arg2 == "dde" || arg2 == "deepin") ctx.distro_name = "deepin";
        else if (arg2 == "devuan") ctx.distro_name = "devuan";
        else if (arg2 == "f" || arg2 == "fe" || arg2 == "fedora") ctx.distro_name = "fedora";
        else if (arg2 == "ft" || arg2 == "funtoo") ctx.distro_name = "funtoo";
        else if (arg2 == "g" || arg2 == "gt" || arg2 == "gentoo") ctx.distro_name = "gentoo";
        else if (arg2 == "k" || arg2 == "kali") ctx.distro_name = "kali";
        else if (arg2 == "mi" || arg2 == "mint") ctx.distro_name = "mint";
        else if (arg2 == "m" || arg2 == "mj" || arg2 == "mjr" || arg2 == "manjaro") ctx.distro_name = "manjaro";
        else if (arg2 == "o" || arg2 == "suse" || arg2 == "opensuse") ctx.distro_name = "opensuse";
        else if (arg2 == "ow" || arg2 == "wrt" || arg2 == "openwrt") ctx.distro_name = "openwrt";
        else if (arg2 == "r" || arg2 == "rasp" || arg2 == "raspios" || arg2 == "raspbian") ctx.distro_name = "raspios";
        else if (arg2 == "s" || arg2 == "sl" || arg2 == "slackware") ctx.distro_name = "slackware";
        else if (arg2 == "u" || arg2 == "ub" || arg2 == "ubuntu") ctx.distro_name = "ubuntu";
        else if (arg2 == "v" || arg2 == "void") ctx.distro_name = "void";
        else if (arg2 == "vnc" || arg2 == "startvnc") ctx.exec_program = "startvnc";
        else if (arg2 == "xs" || arg2 == "xsdl" || arg2 == "xserver" || arg2 == "startxsdl") ctx.exec_program = "startxsdl";
        else if (arg2 == "x11" || arg2 == "x11vnc" || arg2 == "startx11vnc") ctx.exec_program = "startx11vnc";
        else if (arg2 == "no" || arg2 == "novnc") ctx.exec_program = "novnc";
        else ctx.distro_name = arg2;
    }

    // 阶段3: 解析 $3 — 版本代号、架构或快捷程序
    if (pos_args.size() > 2) {
        std::string_view arg3 = pos_args[2];
        if (arg3 == "20.10") ctx.distro_code = "groovy";
        else if (arg3 == "21.04") ctx.distro_code = "hirsute";
        else if (arg3 == "21.10") ctx.distro_code = "impish";
        else if (arg3 == "20.04") ctx.distro_code = "focal";
        else if (arg3 == "18.04") ctx.distro_code = "bionic";
        else if (arg3 == "al" || arg3 == "ap") ctx.distro_code = "alpine";
        else if (arg3 == "8s") ctx.distro_code = "8-Stream";
        else if (arg3 == "9s") ctx.distro_code = "9-Stream";
        else if (arg3 == "s") ctx.distro_code = "sid";
        else if (arg3 == "r" || arg3 == "ro") ctx.distro_code = "rolling";
        else if (arg3 == "raw") ctx.distro_code = "rawhide";
        else if (arg3 == "n" || arg3 == "none") ctx.distro_code = "";
        else if (arg3 == "x" || arg3 == "amd64" || arg3 == "x64") ctx.arch_type = "amd64";
        else if (arg3 == "a" || arg3 == "aarch64" || arg3 == "arm64") ctx.arch_type = "arm64";
        else if (arg3 == "h" || arg3 == "arm" || arg3 == "armhf") ctx.arch_type = "armhf";
        else if (arg3 == "armel") ctx.arch_type = "armel";
        else if (arg3 == "i" || arg3 == "i386" || arg3 == "x86" || arg3 == "x32") ctx.arch_type = "i386";
        else if (arg3 == "p" || arg3 == "ppc") ctx.arch_type = "ppc64el";
        else if (arg3 == "s390x") ctx.arch_type = "s390x";
        else if (arg3 == "m" || arg3 == "mips" || arg3 == "mipsel") ctx.arch_type = "mipsel";
        else if (arg3 == "m64" || arg3 == "mips64" || arg3 == "mips64el") ctx.arch_type = "mips64el";
        else if (arg3.rfind("risc", 0) == 0) ctx.arch_type = "riscv64";
        else if (arg3 == "v" || arg3 == "vnc" || arg3 == "startvnc") ctx.exec_program = "startvnc";
        else if (arg3 == "xs" || arg3 == "xsdl" || arg3 == "xserver" || arg3 == "startxsdl") ctx.exec_program = "startxsdl";
        else if (arg3 == "x11" || arg3 == "x11vnc" || arg3 == "startx11vnc") ctx.exec_program = "startx11vnc";
        else if (arg3 == "no" || arg3 == "novnc") ctx.exec_program = "novnc";
        else if (arg3 == "bash") ctx.tmoe_shell = "/bin/bash";
        else if (arg3 == "ash") ctx.tmoe_shell = "/bin/ash";
        else if (arg3 == "zs") ctx.tmoe_shell = "/bin/zsh";
        else if (arg3 == "z" || arg3 == "zsh") ctx.distro_code = "zsh";
        else ctx.distro_code = arg3;
    }

    // 阶段4: 解析 $4 — 架构、执行程序、软链接或脚本文件（状态反转入口）
    if (pos_args.size() > 3) {
        std::string_view arg4 = pos_args[3];
        if (arg4 == "x" || arg4 == "amd64" || arg4 == "x64") ctx.arch_type = "amd64";
        else if (arg4 == "a" || arg4.rfind("aarch", 0) == 0 || arg4 == "arm64") ctx.arch_type = "arm64";
        else if (arg4 == "h" || arg4 == "arm" || arg4 == "armhf") ctx.arch_type = "armhf";
        else if (arg4 == "armel") ctx.arch_type = "armel";
        else if (arg4 == "i" || arg4 == "i386" || arg4 == "x86" || arg4 == "x32") ctx.arch_type = "i386";
        else if (arg4 == "p" || arg4.rfind("ppc", 0) == 0) ctx.arch_type = "ppc64el";
        else if (arg4.rfind("s390", 0) == 0) ctx.arch_type = "s390x";
        else if (arg4 == "m" || arg4 == "mips" || arg4 == "mipsel") ctx.arch_type = "mipsel";
        else if (arg4 == "m64" || arg4 == "mips64" || arg4 == "mips64el") ctx.arch_type = "mips64el";
        else if (arg4.rfind("risc", 0) == 0) ctx.arch_type = "riscv64";
        else if (arg4 == "v" || arg4 == "vnc" || arg4 == "startvnc") ctx.exec_program = "startvnc";
        else if (arg4 == "xs" || arg4 == "xsdl" || arg4 == "xserver" || arg4 == "startxsdl") ctx.exec_program = "startxsdl";
        else if (arg4 == "x11" || arg4 == "x11vnc" || arg4 == "startx11vnc") ctx.exec_program = "startx11vnc";
        else if (arg4 == "no" || arg4 == "novnc") ctx.exec_program = "novnc";
        else if (arg4 == "bash") ctx.tmoe_shell = "/bin/bash";
        else if (arg4 == "z" || arg4 == "zsh") ctx.tmoe_shell = "/bin/zsh";
        else if (arg4 == "ash") ctx.tmoe_shell = "/bin/ash";
        else if (arg4 == "ln") {
            ctx.create_soft_link = true;
            ctx.link_source_dir = "/etc/profile.d/permanent";
        } else if (is_path_argument(arg4)) {
            ctx.temporary_script_file_01 = arg4;
        } else {
            ctx.exec_program_01 = arg4;
        }
    }

    // 阶段5: 解析 $5 — 依赖阶段4中设定的软链接状态
    if (pos_args.size() > 4) {
        std::string_view arg5 = pos_args[4];
        if (arg5 == "v" || arg5 == "vnc" || arg5 == "startvnc") ctx.exec_program = "startvnc";
        else if (arg5 == "x" || arg5 == "xs" || arg5 == "xsdl" || arg5 == "xserver" || arg5 == "startxsdl") ctx.exec_program = "startxsdl";
        else if (arg5 == "x11" || arg5 == "x11vnc" || arg5 == "startx11vnc") ctx.exec_program = "startx11vnc";
        else if (arg5 == "no" || arg5 == "novnc") ctx.exec_program = "novnc";
        else if (arg5 == "bash") ctx.tmoe_shell = "/bin/bash";
        else if (arg5 == "z" || arg5 == "zsh") ctx.tmoe_shell = "/bin/zsh";
        else if (arg5 == "ash") ctx.tmoe_shell = "/bin/ash";
        else if (arg5 == "ln") {
            ctx.create_soft_link = true;
            ctx.link_source_dir = "/etc/profile.d/permanent";
        } else if (arg5 == "en" || arg5 == "entrypoint") {
            if (ctx.create_soft_link) {
                ctx.link_source_dir = "/usr/local/etc/tmoe-linux/environment/entrypoint";
            } else {
                ctx.exec_program_02 = arg5;
            }
        } else if (is_path_argument(arg5)) {
            if (ctx.create_soft_link) {
                ctx.link_source_dir = arg5;
            } else {
                ctx.temporary_script_file_02 = arg5;
            }
        } else {
            ctx.exec_program_02 = arg5;
        }
    }

    // 阶段6: 解析 $6 — 最后一个参数，同样的状态反转逻辑
    if (pos_args.size() > 5) {
        std::string_view arg6 = pos_args[5];
        if (arg6 == "en" || arg6 == "entrypoint") {
            if (ctx.create_soft_link) {
                ctx.link_source_dir = "/usr/local/etc/tmoe-linux/environment/entrypoint";
            } else {
                ctx.exec_program_03 = arg6;
            }
        } else if (is_path_argument(arg6)) {
            if (ctx.create_soft_link) {
                ctx.link_source_dir = arg6;
            } else {
                ctx.temporary_script_file_03 = arg6;
            }
        } else {
            ctx.exec_program_03 = arg6;
        }
    }

    return ctx;
}

} // namespace tmoe
