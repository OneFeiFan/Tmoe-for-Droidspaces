# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

`tmoes` 是 [tmoe-linux](https://github.com/2moe/tmoe) 的 C++17 子集，目标在 Android [Droidspaces](https://github.com/ravindu644/Droidspaces-OSS) 环境中运行。仅聚焦安卓容器场景的核心功能（容器管理、软件安装、镜像源切换、GUI/桌面环境配置），不会 100% 移植原版。

目前仅在 WSL2 Ubuntu 22.04 LTS 上测试过部分功能。

## 构建与运行

```bash
# 依赖：CMake ≥ 3.12、C++17 编译器（无硬依赖，nlohmann_json 由 FetchContent 自动拉取）

mkdir build && cd build
cmake ..
make -j$(nproc)
```

可选依赖：`-DTMOE_USE_LIBARCHIVE=ON`（tar 原生操作）、`-DTMOE_USE_LIBCURL=ON`（HTTP 下载）。

```bash
./tmoes              # 自动检测平台 → 交互式 TUI
./tmoes m            # 容器管理界面
./tmoes t            # Linux 工具箱界面
./tmoes --lang zh_CN # 指定语言
./tmoes --quiet      # 静默模式
```

## 架构

### 核心层 (`src/core/`)

基础设施，不包含业务逻辑：

- **`Executor`** — 统一的外部命令调用封装。`run()` 逐参数安全转义，`shell()` 用于复杂管道，`passthrough()` 透传终端给交互式子进程。**所有系统调用必须通过 Executor**，不要直接使用 `popen`/`system`。
- **`SystemHelper`** — 文件 I/O、下载（curl→wget→aria2c 回退）、解压、包安装。文件写入操作会通过 `sudo` 提权（项目惯例：需要 root 的操作在命令字符串中显式加 `sudo`）。
- **`TmoeConfig`** — 全局配置 struct，替代 Bash 中的 `$TMOE_*` 变量。`TmoeConfig::detect()` 自动检测运行环境（Termux/WSL/Chroot/Proot/架构等）。
- **`I18n`** — 国际化引擎。使用 `_("key")` 宏翻译，`_f("key", arg1, arg2)` 格式化。翻译 JSON 在 `locales/` 下，编译时由 `tools/embed_i18n.cmake` 嵌入二进制。设置 `TMOE_I18N_CHECK=1` 可收集缺失翻译。
- **`Logger`** — 彩色终端日志 + 进度条。支持 `--quiet`（`Logger::quiet_mode`）和 `--no-color`（`Logger::enable_color`）。
- **`CliParser`** — 将位置参数解析为 `LaunchContext`（实现了原 Bash 脚本的位置状态机逻辑）。
- **`CommandBuilder`** — 构建带安全转义的命令行字符串。

### 领域层 (`src/domain/`)

按业务领域划分：

| 目录 | 职责 | 关键模式 |
|---|---|---|
| `runtime/` | 容器运行时：Proot/Chroot/Nspawn | **策略模式** — `IContainerRuntime` 接口，各引擎独立实现 `start()`/`stop()`/`generate_launch_cmd()` |
| `desktops/` | 桌面环境安装：Xfce/KDE/GNOME/... 共 13+ DE | **工厂模式** — `DesktopBase` 基类定义安装管线（`pre_install_choices` → `post_install_config` → `post_install_extras`），`DesktopFactory` 按 ID 创建实例 |
| `gui_config/` | GUI 元数据注册表 | 数据驱动 — `DesktopInfo` struct 描述每个 DE/WM 的包名、会话命令、发行版差异；`registries.h` 含 70+ WM、主题映射表 |
| `gui/` | GUI/VNC/桌面/美化/远程桌面管理 | `GUIManager` 聚合 `VncManager`、`DesktopManager`、`RemoteDesktopManager`、`BeautificationManager` |
| `system/` | 系统工具 | 镜像源(`MirrorManager`) + 备份(`BackupManager`) + 包管理 + Termux + 环境检测 |
| `apps/` | 应用安装管理 | 浏览器、终端、办公、输入法、开发工具、下载工具 |
| `virtualization/` | Docker + Wine | 容器化环境支持 |
| `features/` | 教育 + Beta 功能 | 实验性/非核心特性 |

### 应用层 (`src/app/`)

- **`Manager`** — 顶层编排器。聚合所有领域模块（`std::unique_ptr`），通过 `tui_routes_` map 将菜单选项路由到处理函数。`run_interactive()` 启动 TUI 循环，`run_launch_context()` 根据 CLI 解析结果执行对应操作。

### 数据文件 (`data/`)

- `rootfs_map.json` — LXC 镜像下载地址映射（分发版 × 版本 × 架构 → rootfs URL 模板）
- `mirror_registry.json` — 镜像源注册表

编译时由 `tools/embed_json.cmake` 嵌入为 C++ 头文件常量。

### 入口流程 (`src/main.cpp`)

7 阶段启动：解析全局标志 → 加载 i18n → 检测环境(`TmoeConfig::detect()`) → CLI 解析(`CliParser::parse()`) → WSL 系统初始化 → 读取持久化 locale 偏好 → 构建 `Manager` 并路由至交互模式或命令分发。

## i18n 工作流

添加新翻译：
1. 在 `locales/` 下创建 `{lang_code}.json`（如 `ja_JP.json`）
2. 文件格式：`{ "key": "translation" }`，key 与现有语言文件对齐
3. 代码中使用 `_("key")` 或 `_f("key", args...)`
4. CMake 检测到 JSON 文件变化后自动重新嵌入

## 关键约定

- **所有系统命令通过 `Executor` 执行**，不要直接调用 `popen`/`system`/`fork`。
- **需要 root 的操作在命令字符串中显式加 `sudo`**（如 `Executor::shell("sudo mkdir -p /etc/...")`）。并非所有操作都需要提权，VNC 启停等以普通用户运行。
- **文件写入使用 `SystemHelper::write_file()`**，该方法内部处理权限。
- **TUI 菜单通过 `whiptail` 渲染**，调用 `Executor::tui_select()` 获取用户选择。
- **新增桌面环境**：创建 `DesktopBase` 子类 → 在 `DesktopFactory` 注册 → 在 `registries.h` 的 `all_desktops()` 添加条目。
- **新增容器运行时**：实现 `IContainerRuntime` 接口 → 在 `Container` 类中使用。
- **代码风格**：命名空间 `tmoe::domain`（领域）/ `tmoe::app`（应用）/ `tmoe`（核心），文件名 snake_case，类名 PascalCase。
