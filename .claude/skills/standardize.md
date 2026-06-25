# standardize — 项目代码标准化流水线

## 概述

对项目中待优化的 `.cpp` 文件执行标准化流水线，依次经过 4 个步骤：
**PackageManager → I18n → Logger → CommandBuilder+Executor**

每一步都有明确的"改什么"和"不该改什么"规则。处理完成后在 `.tmoe-optimized/` 目录下记录文件 hash，后续运行自动跳过无变化的文件。

---

## 核心参考文件（基准，本身不再被处理）

这些是流水线每一步的工具来源，只读不写。**处理待优化文件时，优先使用这些文件中已有的检测方法和配置参数，不要重复造轮子**。

| 文件 | 提供的能力 | 优先替换场景 |
|------|-----------|-------------|
| `src/core/command_builder.hpp/.cpp` | `CommandBuilder` 流式命令构建器 | Step 4：替代所有字符串拼接的命令 |
| `src/core/executor.h/.cpp` | `Executor::shell/passthrough/run/tui_select/has` | Step 4：选用正确的执行方法 |
| `src/core/logger.h/.cpp` | `Logger::info/ok/warn/error/debug/step/press_enter/confirm` | Step 3：统一交互式 I/O |
| `src/core/i18n.h/.cpp` | `_(key)` / `_f(key, args...)` 翻译宏 | Step 2：所有用户可见文本 |
| `src/core/config.h/.cpp` | `TmoeConfig`：`tui_bin`, `install_command`, `remove_command`, `update_command`, `linux_distro`, `is_root`, `is_termux`, `locale` 等 | 替代硬编码的 `"whiptail"`, `"apt install -y"`, 平台检测等 |
| `src/core/launch_context.h` | 强类型 CLI 参数：`exec_program`, `tmoe_shell` 等 | 替代从环境变量/argv 手动解析 |
| `src/core/system_helper.h/.cpp` | 文件 I/O 辅助 | 文件操作标准化 |
| `src/domain/system/package_manager.h/.cpp` | `PackageManager::install/remove/update` 跨发行版包管理 | Step 1：替代原始包管理命令 |

### 配置文件优先原则

处理待优化文件时，遇到以下硬编码值**必须**替换为配置文件中的对应字段：

| 硬编码（❌） | 替换为（✅） | 来源 |
|------------|-----------|------|
| `"whiptail"` / `"dialog"` | `cfg_.tui_bin` | `TmoeConfig` |
| `"apt install -y"` | `cfg_.install_command` 或 `PackageManager::install(...)` | `TmoeConfig` / `PackageManager` |
| `"apt purge -y"` | `cfg_.remove_command` 或 `PackageManager::remove(...)` | `TmoeConfig` / `PackageManager` |
| `cat /etc/os-release \| grep ...` | `cfg_.linux_distro` 或 `infer_family_from_config(...)` | `TmoeConfig` / `package_manager.h` |
| `getenv("HOME")` 散落各处 | 已有 `cfg_` 就不要重复调用 | `TmoeConfig` |
| `Executor::shell("command -v sudo")` | `Executor::has("sudo")` | `Executor` |
| `argv[1]` 手动解析 | `LaunchContext` 结构化字段 | `launch_context.h` |

---

## 执行流程

### 整体策略：先试点、再批量

```
Phase 0: 扫描发现 → 列出全部待处理文件
    ↓
Phase A: 选取 1 个典型文件作为试点 → 完整走 4 步流水线 → 用户审核
    ↓ (用户确认没问题)
Phase B: 剩余文件全部用子代理并行处理（每个文件一个子代理）
    ↓
Phase C: 汇总全部处理结果
```

**重要**：在用户明确确认试点结果之前，不得启动批量处理。

---

### Phase 0: 扫描发现

1. 运行 `git diff --name-only HEAD` 找出有变化的 `.cpp` 文件
2. 排除 `src/core/` 和 `src/domain/system/package_manager.cpp`（它们是基准文件）
3. 读取 `.tmoe-optimized/state.json`，排除 hash 未变的文件
4. 列出待处理文件清单，展示给用户确认
5. 选取 1 个试点文件，其余文件等待用户确认后再处理

### Phase A: 试点 → 用户审核

对试点文件，**严格按顺序**执行以下 4 步。每步完成后展示改动摘要。
用户审核通过后，进入 Phase B。

### Phase B: 批量并行

对剩余所有待处理文件，**每个文件启动一个子代理**并行执行相同的 4 步流水线。
所有子代理使用同一份 skill 规范。

### Phase C: 汇总报告

所有子代理完成后，汇总输出：
- 处理了多少文件
- 每个文件的改动统计（PackageManager 替换数、I18n 新增 key 数、Logger 替换数、CommandBuilder 替换数）
- 写入 `.tmoe-optimized/state.json` 和 hash 文件

---

## Step 1: PackageManager 标准化

### 目标
用 `PackageManager::install/remove/update` 替换原始的系统包管理命令。

### PackageManager API 速查

```cpp
#include "domain/system/package_manager.h"

// 单个包安装（自动处理 eatmydata、失败回退、日志）
PackageManager::install("firefox", family);
// 多包安装
PackageManager::install({"fcitx", "fcitx-config-gtk", "fcitx-im"}, family);
// 卸载
PackageManager::remove("chromium", family);
// 更新索引
PackageManager::update(family);
// 检查命令是否存在
PackageManager::is_command_available("apt");
// 发行版家族枚举
DistroFamily::Debian  // apt/eatmydata
DistroFamily::Arch    // pacman
DistroFamily::RedHat  // dnf/yum
DistroFamily::Suse    // zypper
DistroFamily::Gentoo  // emerge
DistroFamily::Alpine  // apk
// ... 等
// 推断函数（从 distro 名字推断家族）
auto family = infer_family_from_config(cfg_.linux_distro);
```

### ✅ 应该替换的模式

| 原始代码 | 替换为 |
|---------|--------|
| `Executor::passthrough(cfg_.install_command + " pkg")` | `PackageManager::install("pkg", family)` |
| `Executor::passthrough("apt install -y pkg")` | `PackageManager::install("pkg", DistroFamily::Debian)` |
| `Executor::passthrough("pacman -S --noconfirm pkg")` | `PackageManager::install("pkg", DistroFamily::Arch)` |
| `Executor::passthrough("dnf install -y pkg")` | `PackageManager::install("pkg", DistroFamily::RedHat)` |
| `Executor::passthrough("apt purge -y pkg \|\| true")` | `PackageManager::remove("pkg", DistroFamily::Debian)` |
| `Executor::passthrough(cfg_.remove_command + " pkg 2>/dev/null \|\| true")` | `PackageManager::remove("pkg", family)` |
| `Executor::passthrough(cfg_.install_command + " pkg1 pkg2 \|\| true")` | `PackageManager::install({"pkg1", "pkg2"}, family)` |
| `Executor::shell("apt update 2>/dev/null \|\| true")` | `PackageManager::update(DistroFamily::Debian)` |
| `Executor::shell("pacman -Syu --noconfirm")` | `PackageManager::update(DistroFamily::Arch)` |

### ❌ 不该替换的模式（留下不动）

- **Python 包**: `pip install xxx` / `pip3 install xxx`
- **Node 包**: `npm install -g xxx`
- **本地 .deb/.rpm**: `dpkg -i /path/to/local.deb`
- **add-apt-repository**: 这是 PPA 管理，不是包安装
- **apt-mark**: 这是包锁定/解锁，不属于 install/remove
- **发行版特定正则**: `apt purge -y firefox ^firefox-locale`（`^` 是 apt 专有语法）
- **snap/flatpak**: 非标准包管理器
- **zypper refresh / dnf config-manager**: 仓库配置，不是包操作

---

## Step 2: I18n 国际化

### 目标
所有用户可见文本（UI 菜单、提示、错误消息、Logger 输出）提取为 `_(key)` 宏，
翻译写入 `locales/zh_CN.json`。

### I18n API 速查

```cpp
#include "core/i18n.h"

// 简单翻译（返回 std::string，不需要额外包裹）
_("module.some_key")           // → 查找 zh_CN.json 中 "module.some_key" 的值
// 带参数的翻译
_f("backup.complete", name, size)  // → 替换 {0} {1} 占位符
```

### 翻译文件格式 (`locales/zh_CN.json`)

```json
{
  "module.feature.element": "简体中文文本",
  "backup.complete": "备份完成: {0} ({1})"
}
```

- key 只包含小写字母、数字、点、下划线
- 占位符用 `{0}`, `{1}`, `{2}` ...

### Key 命名规范

```
模块名.功能描述.元素（可选）
```

示例：
- `browser.menu_title` — 浏览器菜单标题
- `browser.install_firefox` — 安装 Firefox 的步骤提示
- `devtools.vscode.install` — VS Code 安装相关
- `input.fcitx4_core` — fcitx4 核心输入法
- `swcenter.electron_apps` — 软件中心 Electron 应用区
- `docker.container_started` — Docker 容器已启动

### ✅ 需要提取的文本范围

- **UI 菜单文本**：TUI 对话框标题、提示、按钮标签
- **用户提示信息**：`Logger::info/warn/error/ok` 中的消息
- **步骤信息**：`Logger::step(...)` 的内容
- **确认对话框**：`Logger::confirm(...)` 的问题文本
- **错误消息**：显示给用户看的错误描述

### ❌ 不需要提取的文本

- **调试日志**：`Logger::debug(...)` — 只在开发时可见
- **命令字符串**：传给 shell 的命令行文本
- **文件路径**：`/usr/local/bin/...`
- **包名**：`firefox-esr`, `chromium`
- **URL**：下载地址
- **代码注释**
- **配置键/值**

### 双语文本拆解规则

当一个字符串同时包含中文和英文：

**情况 A — 两种语言表达相同意思** → 保留中文，去掉英文。

```
改前: "请从两个小可爱中选择一个 (Choose from two)"
改后: _("browser.choose_one")
      → zh_CN.json: "请从两个小可爱中选择一个"
```

**情况 B — 只有英文** → **先翻译成中文**，再作为 source text。

```
改前: "Which browser do you want to install?"
改后: _("browser.menu_prompt")
      → zh_CN.json: "您想安装哪个浏览器？"
```

**情况 C — 两种语言表达不同信息**（极少见）→ **拆分**为两个独立 key。

### 表情符号保留规则

表情符号是 UI 的一部分，**必须保留原位不丢失**。处理方式：表情符放在翻译文本的最前面。

```
改前: "🎵 music:以雅以南,以龠不僭"
      → 中文为主，🎵 保留在译文前
改后: _("electron.music")
      → zh_CN.json: "🎵 音乐：以雅以南，以龠不僭"

改前: "🇻 🇸 Visual Studio Code (现代化代码编辑器)"
      → 🇻 🇸 保留在译文前
改后: _("devtools.vscode")
      → zh_CN.json: "🇻 🇸 Visual Studio Code（现代化代码编辑器）"

改前: "✓ 安装完成"  或  "✗ 安装失败"
      → ✓ ✗ 本身就是 Logger::ok/error 输出的，不需要放 i18n 里
```

### 翻译文本优先级（处理决策树）

```
原始文本
  ├─ 包含中文？
  │   ├─ 同时有英文表达相同意思 → 保留中文，去掉英文 → _(key)
  │   ├─ 同时有英文表达不同意思 → 拆分为两个 key → _(key1) + _(key2)
  │   └─ 只有中文               → 直接作为 source text → _(key)
  └─ 只有英文？
      └─ 先翻译成中文 → 作为 source text → _(key)
  
不论哪种情况：表情符号保留，放在译文最前面。
```

### `_()` 宏不需要额外包裹

```cpp
// ❌ 错误：_() 已经返回 std::string，不需要再包一层
std::string menu = std::string(_("browser.title"));

// ✅ 正确
std::string menu = _("browser.title");

// ✅ 正确：和其他字符串拼接
std::string menu = cfg_.tui_bin + " --title \"" + _("browser.title") + "\"";

// ✅ 正确：传给 Logger
Logger::info(_("browser.install_complete"));
Logger::step(_("browser.installing"));
```

---

## Step 3: Logger 标准化

### 目标
统一项目的交互式输入输出风格，用 Logger 方法替换 `std::cout`, `printf`, `fprintf` 等原始输出。

### Logger API 速查

```cpp
#include "core/logger.h"

Logger::info(msg)      // 普通信息
Logger::ok(msg)        // ✓ 成功信息（绿色）
Logger::warn(msg)      // ⚠ 警告（黄色）
Logger::error(msg)     // ✗ 错误（红色）
Logger::debug(msg)     // 调试（仅在 debug 模式显示）
Logger::step(msg)      // " => " 步骤指示符
Logger::press_enter()  // "按回车继续..."
Logger::confirm(question)  // y/n 询问，返回 bool
Logger::progress(pct, label)  // 进度条
Logger::ok_or_fail(ok, task)  // 根据布尔值输出 ✓ 或 ✗
```

### ✅ 应该替换的模式

| 原始代码 | 替换为 |
|---------|--------|
| `std::cout << "xxx" << std::endl;` | `Logger::info("xxx")` 或 `Logger::info(_("key"))` |
| `std::cerr << "error" << std::endl;` | `Logger::error(_("key"))` |
| `printf("xxx\n");` | `Logger::info(_("key"))` |
| `fprintf(stderr, "xxx");` | `Logger::error(_("key"))` |
| `std::cout << " => " << msg << std::endl;` | `Logger::step(msg)` |
| `system("pause");` 或 `getchar();` | `Logger::press_enter()` |

### ❌ 不该替换的模式

- 写入文件的 `std::ofstream` 操作
- 配置文件内容生成（写入字符串流再写文件）
- 命令输出的捕获（`Executor::shell(...).stdout_data`）

### 级别选择指南

| 场景 | 使用 |
|------|------|
| 操作完成、安装成功 | `Logger::ok(...)` |
| 常规提示、状态说明 | `Logger::info(...)` |
| 潜在问题、非致命错误 | `Logger::warn(...)` |
| 操作失败、致命错误 | `Logger::error(...)` |
| 步骤进度 | `Logger::step(...)` |
| 需要用户确认 | `Logger::confirm(...)` |

---

## Step 4: CommandBuilder + Executor 标准化

### 目标
用 `CommandBuilder` 流式构建 + `Executor` 正确方法替换原始字符串拼接的 shell 命令。

### CommandBuilder API 速查

```cpp
#include "core/command_builder.hpp"

CommandBuilder(bin)                  // bin = 可执行文件名
  .add_arg(arg)                      // 位置参数（自动 shell 转义）
  .add_arg_if(cond, arg)             // 条件参数
  .add_flag(flag)                    // 布尔标志（-y, --verbose, -p）
  .add_flag_if(cond, flag)           // 条件标志
  .add_kv(key, value)                // --key=value 风格
  .add_kv_if(cond, key, value)       // 条件 kv
  .add_opt(opt, value)               // --opt value 风格（两个独立参数）
  .add_opt_if(cond, opt, value)      // 条件 opt
  .add_bind(source, dest)            // -b source:dest（proot 风格挂载）
  .add_bind_if(cond, src, dst)       // 条件 bind
  .add_bind_to(prefix, src, dst)     // 自定义挂载前缀（"--bind=", "-v " 等）
  .add_bind_to_if(cond, pre, s, d)   // 条件自定义挂载
  .add_env(key, value)               // 环境变量（前置 KEY=VALUE）
  .set_prefix(prefix)                // 命令前缀（sudo, nice 等）
  .add_raw(text)                     // 原始文本（不转义！用于 2>/dev/null, 管道, || true）
  .build_string()                    // → std::string 完整命令
  .execute()                         // → build_string + Executor::shell
```

### Executor 方法选择指南

| 方法 | 使用场景 |
|------|---------|
| `Executor::shell(cmd)` | 需要捕获输出（stdout/stderr），非交互命令 |
| `Executor::passthrough(cmd)` | 需要用户看到实时输出（安装进度条、下载进度），交互式命令 |
| `Executor::run(bin, {args...})` | 逐参数安全调用（不使用 CommandBuilder 时） |
| `Executor::tui_select(args)` | whiptail/dialog 对话框 |
| `Executor::has(bin)` | 检查命令是否在 PATH 中 |

### ✅ 应该替换的模式

| 原始代码 | 替换为 CommandBuilder |
|---------|----------------------|
| `Executor::shell("mkdir -p " + dir)` | `CommandBuilder("mkdir").add_flag("-p").add_arg(dir)` |
| `Executor::shell("chmod 755 " + path)` | `CommandBuilder("chmod").add_arg("755").add_arg(path)` |
| `Executor::shell("rm -rf " + dir + " 2>/dev/null")` | `CommandBuilder("rm").add_flag("-rf").add_arg(dir).add_raw("2>/dev/null")` |
| `Executor::shell("cp -f " + src + " " + dst)` | `CommandBuilder("cp").add_flag("-f").add_arg(src).add_arg(dst)` |
| `Executor::shell("ln -sf " + target + " " + link)` | `CommandBuilder("ln").add_flag("-sf").add_arg(target).add_arg(link)` |
| `Executor::passthrough("apt update 2>/dev/null \|\| true")` | `CommandBuilder("apt").add_flag("update").add_raw("2>/dev/null \|\| true")` |
| `Executor::passthrough("rpm --import " + url)` | `CommandBuilder("rpm").add_flag("--import").add_arg(url)` |
| `Executor::passthrough("apt purge -y " + pkg)` | `CommandBuilder("apt").add_flag("purge").add_flag("-y").add_arg(pkg)` |
| `Executor::shell("pkill " + name + " 2>/dev/null \|\| true")` | `CommandBuilder("pkill").add_arg(name).add_raw("2>/dev/null \|\| true")` |

### `add_raw` 使用规则（重要！）

`add_raw` 的内容**不经过 shell 转义**，直接拼接到命令末尾。适用场景：
- `"2>/dev/null"` — 重定向
- `"2>/dev/null || true"` — 忽略错误
- `"| grep -q pattern"` — 管道
- `">> /path/file"` — 追加重定向

**注意**：`add_raw` 拼接时前面自动加一个空格，所以不需要手动加。

### ❌ 不该替换的模式

- **Shell 多行脚本**: `if/fi`, `for/done`, `while` 块
- **Heredoc**: `cat > file << 'EOF' ... EOF`
- **复杂管道**: 含 `&&`, `;`, 多个 `|` 的命令链
- **Shell 变量**: `"${HOME}/Desktop"` 等需要 shell 展开的字符串

### 执行方法选择

```cpp
// 需要用户看到实时输出（安装进度、下载进度）→ passthrough
Executor::passthrough(cb.build_string());

// 需要捕获输出或静默执行 → shell
auto result = Executor::shell(cb.build_string());

// 简单执行，不关心输出 → execute()
cb.execute();

// 复杂命令不需要 CommandBuilder → 直接 shell
Executor::shell("dpkg -l firefox 2>/dev/null | grep -q '^ii'");
```

---

## 全局规则

### 每个文件处理前

1. 添加缺失的 `#include`：
   - 用了 PackageManager → `#include "domain/system/package_manager.h"`
   - 用了 CommandBuilder → `#include "core/command_builder.hpp"`
   - 用了 I18n → `#include "core/i18n.h"`
   - 用了 Logger → `#include "core/logger.h"`（通常已存在）
2. 读取文件全文，理解上下文
3. 记录文件当前的 git blob hash：`git hash-object <file>`

### 每个文件处理后

1. 写入 `.tmoe-optimized/hashes/<relative-path>.hash`（内容为 git blob hash）
2. 更新 `.tmoe-optimized/state.json` 的 `files` 记录

### 不处理的情况

1. 文件在 `.tmoe-optimized/` 中有 hash 记录，且当前 git blob hash 与记录一致
2. 文件在 `src/core/` 目录下
3. 文件是 `src/domain/system/package_manager.cpp`
4. 头文件（`.h`, `.hpp`）— 除非对应的 `.cpp` 处理中需要修改接口签名

---

## 完整示例：一个函数的前后对比

### 改前

```cpp
void BrowserManager::install_firefox_esr() {
    Logger::step("Firefox ESR");
    auto family = infer_family_from_config(cfg_.linux_distro);
    bool ok = false;
    switch (family) {
        case DistroFamily::Debian:
            ensure_firefox_ppa();
            ok = PackageManager::install("firefox-esr", family);
            if (ok) Executor::passthrough(
                cfg_.install_command + " ffmpeg ^firefox-esr-locale-zh || true");
            break;
        // ...
    }
    if (!ok) {
        std::cout << "Firefox ESR 安装失败，尝试安装普通 Firefox..." << std::endl;
        install_firefox();
        return;
    }
    Executor::shell("chmod 755 /usr/local/bin/firefox-esr--no-sandbox");
    std::cout << " => 创建 no-sandbox 快捷方式" << std::endl;
}
```

### 改后

```cpp
void BrowserManager::install_firefox_esr() {
    Logger::step(_("browser.firefox_esr_installing"));  // I18n
    auto family = infer_family_from_config(cfg_.linux_distro);
    bool ok = false;
    switch (family) {
        case DistroFamily::Debian:
            ensure_firefox_ppa();
            ok = PackageManager::install("firefox-esr", family);
            if (ok) PackageManager::install({"ffmpeg", "^firefox-esr-locale-zh"}, family);  // PM
            break;
        // ...
    }
    if (!ok) {
        Logger::warn(_("browser.firefox_esr_failed"));  // Logger + I18n
        install_firefox();
        return;
    }
    Executor::shell(  // CommandBuilder
        CommandBuilder("chmod").add_arg("755")
            .add_arg("/usr/local/bin/firefox-esr--no-sandbox").build_string());
    Logger::step(_("browser.no_sandbox_wrapper"));  // Logger + I18n
}
```

### locales/zh_CN.json 新增条目

```json
{
  "browser.firefox_esr_installing": "正在安装 Firefox ESR",
  "browser.firefox_esr_failed": "Firefox ESR 安装失败，尝试安装普通 Firefox..."
}
```

---

## Tracking 文件格式

### `.tmoe-optimized/state.json`

```json
{
  "version": 1,
  "last_full_scan": "2026-06-25T12:00:00Z",
  "files": {
    "src/domain/apps/browser.cpp": {
      "hash": "a1b2c3d4e5f6...",
      "last_processed": "2026-06-25T12:30:00Z",
      "steps_completed": ["pm", "i18n", "logger", "cmdbuilder"]
    }
  }
}
```

### 跳过逻辑

```
if (file.hash == state.files[file].hash) → 跳过（无变化）
if (file 在 core/ 或 package_manager.cpp) → 跳过（基准文件）
else → 处理
```
