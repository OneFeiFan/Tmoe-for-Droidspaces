#ifndef LAUNCH_CONTEXT_H
#define LAUNCH_CONTEXT_H
#pragma once

#include <string>
#include <vector>

namespace tmoe {

    /** 程序启动模式路由。 */
    enum class LaunchMode {
        Interactive,      // 默认交互式 TUI 菜单
        Proot,            // PRoot 容器模式
        Chroot,           // Chroot 容器模式
        Nspawn,           // systemd-nspawn 容器模式
        ListContainers,   // ls: 列出已安装容器
        ZshManager,       // zsh 管理器
        ThemeManager,     // 主题配置
        Aria2Manager,     // Aria2 管理器
        DebianInstall,    // 一键安装
        ManagerMenu,      // 管理菜单跳转
        ToolMenu,         // 工具菜单跳转
        MirrorManager,    // 镜像源管理 (-m / --mirror)
        GuiManager,       // GUI 图形界面管理 (对应旧 Bash gui_main)
        Help              // 帮助/用法提示
    };

    /** 强类型启动上下文（高内聚）。 */
    struct LaunchContext {
        LaunchMode mode = LaunchMode::Interactive;
        bool needs_root = true;     // 默认需要 root（安全优先）；白名单操作显式关闭

        std::string distro_name;    // 发行版名称 (对应原 $2)
        std::string distro_code;    // 版本代号 (对应原 $3)
        std::string arch_type;      // CPU 架构类型 (对应原 $3/$4)
        std::string exec_program;   // 核心图形或执行程序 (如 startvnc/novnc)
        std::string tmoe_shell;     // 指定启动的 Shell (如 /bin/bash)

        // 软链接机制状态
        bool create_soft_link = false;
        std::string link_source_dir;

        // 多段命令投递存储 ($4, $5, $6)
        std::string exec_program_01;
        std::string exec_program_02;
        std::string exec_program_03;

        // 多段脚本文件路径存储 (支持跨平台绝对/相对路径)
        std::string temporary_script_file_01;
        std::string temporary_script_file_02;
        std::string temporary_script_file_03;

        std::string gui_flag;       // GUI CLI 标志 (--auto-install-gui-xfce 等)
        std::string raw_parameters; // 原始参数字符串 (Bash $*)
    };

} // namespace tmoe
#endif //LAUNCH_CONTEXT_H
