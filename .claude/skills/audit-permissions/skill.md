## 概述

对项目中所有写系统路径或执行特权命令的操作进行权限审计。
**不使用正则脚本**——脚本无法穿透字符串拼接、变量追踪、CommandBuilder 链。
由 Agent 直接读取源码，**根据上下文语义判断**每个操作是否需要 sudo。

---

## 审计原则

1. **不准依赖脚本输出做最终判断**——脚本只能辅助定位可疑行，不能给出结论
2. **逐个文件、逐个操作审计**——Agent 读取每个 `.cpp` 文件，检查每个可疑操作
3. **上下文决定一切**——同一个 `rm` 命令，看删的是 `/tmp/` 还是 `/etc/` 决定要不要 sudo

---

## 需要审计的操作类型

### A. Executor 调用（passthrough / shell / run）

```cpp
Executor::passthrough("cmd");
Executor::shell("cmd");
Executor::run("bin", {...});
```

Agent 需要：
- 展开字符串拼接，还原完整命令
- 如果包含变量（`"apt install " + pkg`），追踪变量定义
- 判断命令是写系统路径还是只读
- 检查是否已有 `sudo` 前缀

### B. CommandBuilder 链

```cpp
CommandBuilder("rm").add_flag("-rf").add_arg("/opt/foo").execute();
CommandBuilder("chmod").add_arg("755").add_arg("/usr/local/bin/bar").execute();
```

Agent 需要：
- 查看 `.add_arg()` 中是否有系统路径
- 检查是否有 `.set_prefix("sudo")`
- 判断操作是否需要特权

### C. C++ 文件操作

```cpp
fs::create_directories("/etc/foo");
fs::copy("/tmp/a", "/usr/local/bin/b");
std::ofstream ofs("/etc/config.conf");
SystemHelper::write_file(path, content);  // 已自动提权，安全
```

Agent 需要：
- 检查目标路径是否为系统路径
- `SystemHelper::write_file/append_file` 已处理提权，无需标记
- 其余直接 C++ 文件操作需要 sudo 方案

---

## 审计规则

### ✅ 需要 sudo

| 场景 | 示例 |
|------|------|
| 包管理写操作 | `apt install/purge`, `dpkg -i/--configure`, `pacman -S`, `yum/dnf install`, `apk add/del`, `emerge`, `zypper in/rm` |
| 系统服务管理 | `systemctl start/stop/enable/disable/restart`, `service ... start/stop` |
| 写系统配置文件 | `sed -i /etc/...`, `tee /etc/...`, `cat > /etc/...` |
| 系统路径文件操作 | `rm/mv/cp/mkdir/chmod/chown` 目标为 `/etc/`, `/usr/`, `/opt/`, `/var/lib/`, `/var/cache/`, `/boot/` |
| 解压到系统路径 | `tar -C /usr/`, `tar -C /opt/` |
| 创建系统路径符号链接 | `ln -sf /usr/local/bin/...` |
| 写引导/磁盘 | `dd of=/dev/...` |
| PPA 管理 | `add-apt-repository` |

### ❌ 不需要 sudo

| 场景 | 原因 |
|------|------|
| 只读查询 | `dpkg --print-architecture`, `dpkg -l`, `dpkg --audit`, `apt-cache show`, `systemctl status`, `grep`, `cat`（只读）, `ls`, `find`, `which` |
| 交互编辑器 | `nano /etc/...`, `${EDITOR:-nano}` —— 用户手动保存 |
| 用户目录 | `~/`, `$HOME/` —— **绝不能 sudo**，否则归属错乱 |
| /tmp 操作 | 任何用户可写 |
| 纯显示 | `neofetch`, `lolcat`, `echo` |
| 容器 rootfs | `root + "/usr/..."` —— 不是宿主机路径 |
| Termux | 进程已 root |

### ⚠️ 特别注意

1. **重定向陷阱**：`sudo cmd > /etc/file` 中 `>` 在当前 shell 执行，不受 sudo 保护。必须用 `sudo sh -c 'cmd > /etc/file'` 或 `| sudo tee`
2. **混合路径**：`rm -rf ~/.vnc /etc/tigervnc` 必须拆分为两条命令
3. **CommandBuilder**：用 `CommandBuilder("sudo").add_arg("rm")...` 或 `.set_prefix("sudo")`

---

## 执行流程

### 完整审计

```bash
# Agent 执行以下命令列出所有 Executor 调用：
grep -rn "Executor::passthrough\|Executor::shell\|Executor::run" src/ --include="*.cpp"

# Agent 逐个文件读取，对每个调用：
# 1. 如果有字符串拼接，追踪变量定义，还原完整命令
# 2. 如果有 CommandBuilder，查看 add_arg 目标路径
# 3. 判断是否需要 sudo
# 4. 需要 sudo 但未加 → 直接修复
```

### 增量审计（推荐日常使用）

```bash
# 只看本次改动的文件
git diff --name-only HEAD | grep '\.cpp$' | while read f; do
    # Agent 读取 $f，审计其中新增/修改的 Executor/CommandBuilder/fs 调用
done
```

### 按操作类型定向审计（最精准）

当出现特定类型报错时，直接 grep 该操作：

```bash
# 例如用户报 "tar: 无法 mkdir: 权限不够"
grep -rn "tar.*-C /usr\|tar.*-C /opt\|tar.*-C /etc" src/ --include="*.cpp" | grep -v sudo

# 例如用户报 "dpkg: 权限不够"
grep -rn '"dpkg -i\|"rpm -\|\"apt install' src/ --include="*.cpp" | grep -v sudo
```

然后 Agent 直接读相关文件，上下文判断，修复。

---

## 禁止事项

- ❌ 不要在 Agent 内部调用 `python tools/audit_final.py` 并相信其输出
- ❌ 不要运行脚本后直接说"剩余都是误报"
- ❌ 不要批量 `replace_all` 加 sudo 而不检查上下文
- ✅ 读源码、理解语义、逐个判断、逐个修复
