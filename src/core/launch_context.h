//
// Created by 30225 on 2026/5/25.
//

#ifndef LAUNCH_CONTEXT_H
#define LAUNCH_CONTEXT_H
#pragma once
#include <string>
#include <vector>

namespace tmoe {

    /// 明确的主程序运行模式路由
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
        Help              // 帮助信息提示
    };

    /// 强类型启动上下文对象 (高内聚)
    struct LaunchContext {
        LaunchMode mode = LaunchMode::Interactive;

        std::string distro_name;    // 发行版名称 (对应原 $2 或降级后的参数)
        std::string distro_code;    // 版本代号 (对应原 $3)
        std::string arch_type;      // CPU 架构类型 (对应原 $3/$4)
        std::string exec_program;   // 核心图形或执行程序 (如 startvnc/novnc)
        std::string tmoe_shell;     // 指定要运行的 Shell 环境 (如 /bin/bash)

        // 一键软链接机制状态
        bool create_soft_link = false;
        std::string link_source_dir;

        // 动态多段命令投递存储 ($4, $5, $6)
        std::string exec_program_01;
        std::string exec_program_02;
        std::string exec_program_03;

        // 动态多段脚本文件投递存储 (支持跨平台绝对路径/相对路径分流)
        std::string temporary_script_file_01;
        std::string temporary_script_file_02;
        std::string temporary_script_file_03;

        std::string raw_parameters; // 保留原始拼装参数串 (对应 Bash 里的 $*)
    };

} // namespace tmoe
#endif //LAUNCH_CONTEXT_H
