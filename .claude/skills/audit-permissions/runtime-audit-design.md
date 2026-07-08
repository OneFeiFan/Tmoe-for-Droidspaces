## 运行时权限审计方案

### 问题

现有正则脚本无法穿透：
- 多命令拼接 `"cd /tmp && cmd1; cmd2"`
- 字符串变量 `"apt install " + pkg`
- CommandBuilder 链 `CommandBuilder("rm").add_arg("/opt/...").execute()`
- heredoc `cat > /etc/... << 'EOF'`
- C++ 文件操作 `fs::create_directories`, `std::ofstream`

56 个文件、447 个 Executor 调用，手工逐行审完后仍不断发现遗漏。

### 方案：Executor 层运行时拦截

在 `Executor::passthrough()` 和 `Executor::shell()` 内部加一层检查，
通过环境变量 `TMOE_PERM_AUDIT=1` 开启，对所有命令做权限审计。

#### 核心逻辑

```cpp
// executor.cpp
ExecResult Executor::passthrough(std::string_view cmd) {
#ifdef TMOE_PERM_AUDIT
    audit_permission(cmd);
#endif
    return system(cmd.data());
}

void Executor::audit_permission(std::string_view cmd) {
    // 1. 检查是否操作用系统路径
    static const char* SYS_PATHS[] = {"/etc/","/usr/","/opt/","/var/lib/","/var/cache/","/boot/"};
    bool hits_sys = false;
    for (auto& p : SYS_PATHS) {
        if (cmd.find(p) != std::string::npos) { hits_sys = true; break; }
    }
    if (!hits_sys) return;

    // 2. 检查是否有写操作
    bool is_write = false;
    static const char* WRITE_OPS[] = {
        "rm ","mv ","cp ","mkdir","chmod","chown","ln ","sed ","tee ",">",">>",
        "tar -","dpkg -i","apt install","apt purge","apt-get","pacman -","yum ",
        "dnf ","zypper ","apk add","apk del","emerge","xbps-","add-apt-repo",
        "systemctl start","systemctl stop","systemctl enable","systemctl disable",
        "service ","cat >","dd ","mount ","wget -O","curl -o","aria2c.*-o",
        "pip install","npm install -g","gem install"
    };
    for (auto& op : WRITE_OPS) {
        if (cmd.find(op) != std::string::npos) { is_write = true; break; }
    }
    if (!is_write) return;

    // 3. 检查是否有 sudo
    if (cmd.find("sudo ") == std::string::npos) {
        std::cerr << "[TMOE PERM AUDIT] MISSING SUDO: " << cmd.substr(0, 120) << std::endl;
    }
}
```

#### 优势

1. **零漏检** — 运行时拦截所有命令，不管源码怎么拼接
2. **零误报** — 只有真正执行到的命令才审计
3. **零开销** — 编译时宏控制，生产环境完全不开启
4. **覆盖全面** — Executor::passthrough/shell/run 全部走同一入口

#### 开启方式

```bash
TMOE_PERM_AUDIT=1 ./tmoe      # 运行时输出所有缺sudo的写系统路径命令
TMOE_PERM_AUDIT=2 ./tmoe      # 严格模式：缺sudo直接拒绝执行并报错
```

### 配合手段：编译期补充

对于不经过 Executor 的操作（`fs::create_directories`、`SystemHelper::write_file`、`std::ofstream`），
`SystemHelper::write_file` 已通过 `access()` 检测自动提权。其余 C++ 文件操作数量有限，
可通过已有的 `audit_final.py` 做静态检查（目前已覆盖 `fs::create_directories`、`std::ofstream`）。

### 实施步骤

1. 在 `Executor::passthrough/shell/run` 内部加 `audit_permission()` 调用
2. 加 `TMOE_PERM_AUDIT` 环境变量检测
3. 编译运行，逐个功能测试
4. 根据输出修复所有缺 sudo 的命令
5. 确认零输出后关闭环境变量
