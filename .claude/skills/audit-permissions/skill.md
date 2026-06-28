## 概述

对项目中所有 `Executor::passthrough/shell/run` 调用进行**双向权限审计**，确保：
- 系统级操作（`/etc/`, `/usr/`, `/opt/`, 包管理, systemd）已通过 `sudo` 提权
- 用户级操作（`~/`, `$HOME/`）**不**通过 `sudo` 执行，避免文件归属错乱

审计工具位于 `tools/audit_final.py`，检测两类问题：**缺少 sudo** 和 **用户路径上多余 sudo**。

---

## 核心参考文件

| 文件 | 作用 |
|------|------|
| `tools/audit_final.py` | 主审计脚本，检测 missing + unnecessary sudo |
| `tools/add_sudo.py` | 批量自动补 sudo 的工具（谨慎使用，需人工复核） |
| `src/core/executor.h/.cpp` | `Executor::shell/passthrough/run` 所有命令入口点 |
| `src/core/command_builder.hpp` | `CommandBuilder::set_prefix("sudo")` 可替代字符串拼接 `sudo` |

---

## 权限规则

### ✅ 应该加 sudo 的场景（系统级操作）

| 命令类型 | 示例 |
|---------|------|
| 包管理写操作 | `dpkg --configure -a`, `dpkg --add-architecture`, `dpkg-reconfigure` |
| 系统服务管理 | `systemctl start/stop/enable/disable/restart/daemon-reload`, `service ... start/stop` |
| 系统配置文件修改 | `sed -i ... /etc/apt/sources.list`, `sed -i ... /etc/pacman.conf` |
| 系统路径写入 | `rm -rf /etc/...`, `cp -f ... /etc/...`, `mv ... /usr/share/...` |
| 权限修改 | `chmod` 作用于 `/etc/`, `/usr/local/bin/` 等系统路径 |
| 引导/磁盘操作 | `dd if=... of=...` 写入块设备 |
| PPA 管理 | `add-apt-repository` |
| 下载到系统路径 | `wget -O /usr/local/bin/...`, `curl -o /etc/...` |
| Shell 重定向写系统文件 | `sudo sh -c 'echo ... > /etc/...'`（注意 `>` 重定向不能直接用 `sudo cmd > /etc/...`） |

### ❌ 不应该加 sudo 的场景（用户级操作或安全操作）

| 命令类型 | 示例 | 原因 |
|---------|------|------|
| 只读查询 | `dpkg --print-architecture`, `dpkg --audit`, `dpkg-query -W`, `fuser`（不带 `-k`） | 不需要写权限 |
| 服务状态查询 | `systemctl status`, `systemctl is-enabled`, `systemctl list-units` | 只读 |
| 交互编辑器 | `nano /etc/...`, `${EDITOR:-nano} /etc/...` | 用户手动编辑，保存时会触发编辑器的 sudo 机制 |
| 用户目录操作 | `rm -rf ~/.vnc`, `cp ~/...`, `mkdir ~/.config/...` | **绝不能 sudo**，否则文件归属 root，普通用户无法访问 |
| 显示/输出 | `neofetch`, `lolcat`, `cat /etc/os-release` | 纯输出，不需写权限 |
| 目录切换 | `cd /usr/share/...` | 无副作用 |
| 进程查询 | `pgrep`, `pkill`（非特权进程） | 普通用户可执行 |
| 用户态服务 | `/usr/local/bin/code-server &` 等后台服务 | 应作为用户进程运行 |

### ⚠️ 特别注意：重定向陷阱

```bash
# ❌ 错误：sudo 只作用于 cat，> 重定向在当前 shell（非 sudo）执行
sudo cat > /etc/rc.local <<'EOF'
...

# ✅ 正确：整个写入都在 sudo 下
sudo sh -c 'cat > /etc/rc.local' <<'EOF'
...
```

### ⚠️ 特别注意：混合路径

```bash
# ❌ 问题：~/.vnc 是用户路径（不应 sudo），/etc/tigervnc 是系统路径（需要 sudo）
rm -rf ~/.vnc /etc/tigervnc ~/.dbus ~/.cache/sessions

# ✅ 拆分：
rm -rf /etc/tigervnc              # → sudo rm -rf /etc/tigervnc
rm -rf ~/.vnc ~/.dbus ~/.cache    # → 不 sudo
```

---

## 执行流程

### Step 1: 运行审计

```bash
python tools/audit_final.py
```

输出分为两组：
- `MISSING SUDO` — 系统级操作缺少 sudo
- `UNNECESSARY SUDO on USER PATHS` — 用户路径上错误使用了 sudo

### Step 2: 分析每个条目

对每个 `MISSING SUDO` 条目，判断属于哪类：

| 类别 | 处理方式 |
|------|---------|
| 确实需要 sudo | 在命令前加 `sudo ` |
| 只读/查询 | 无需处理，审计脚本会跳过 |
| 交互编辑器 | 无需处理 |
| 混合路径 | 拆分为系统路径（加 sudo）和用户路径（不加 sudo） |

对每个 `UNNECESSARY SUDO` 条目：
- 移除 `sudo` 前缀
- 如果需要写用户目录，确保后续有 `chown` 修正归属（参考 `SystemHelper::fix_user_home_ownership()`）

### Step 3: 手动修复

逐个文件修复。优先处理**缺少 sudo 的系统操作**，再处理**用户路径多余 sudo**。

### Step 4: 复核

```bash
python tools/audit_final.py   # 确认 0 问题
```

---

## 批量工具使用规则

`tools/add_sudo.py` 可以批量给系统命令加 sudo，但**必须遵守**：

1. 运行前先 `git stash` 保存当前状态
2. 运行 `python tools/add_sudo.py`
3. `git diff` 审查所有改动
4. 人工确认每条改动合理
5. 运行 `python tools/audit_final.py` 确认结果
6. 如有误加，`git checkout -- <file>` 回退

**不要**在不审查 diff 的情况下直接提交批量工具的改动。

---

## 常见审计结果解读

### `dpkg --print-architecture` / `dpkg --audit` / `dpkg-query`
→ 只读查询，**不需要 sudo**。审计脚本会自动排除。

### `nano /etc/...` / `${EDITOR:-nano} /etc/...`
→ 交互编辑器，用户手动编辑时保存，编辑器自身会处理权限。**不需要加 sudo**。

### `systemctl status`
→ 状态查询，只读。**不需要 sudo**。而 `systemctl start/stop/enable` 需要。

### `rm -rf ~/.vnc /etc/tigervnc`
→ 混合路径。`/etc/tigervnc` 需要 sudo，`~/.vnc` 不能 sudo。应该**拆分为两条命令**。

### `fuser /var/lib/dpkg/lock`
→ 不带 `-k` 时只显示占用者，只读。**不需要 sudo**。

---

## 哲学

**权限最小化**：只对真正需要特权的操作使用 sudo。多余的 sudo 会导致文件归属错乱（用户目录下的文件变成 root 所有），反过来影响普通用户使用。

**审计双向**：不仅要找"该提权没提权"，也要找"不该提权却提权"。两种错误的后果同样严重。

**工具辅助，人工决策**：脚本提供候选列表，但不能自动判断。每条改动都需要人确认上下文是否正确。
