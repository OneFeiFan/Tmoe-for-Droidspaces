# tmoes — tmoe for Droidspaces ~~(划掉)~~

> tmoe-linux 的 C++17 现代化子集。

> ~~Bash 胶水 → 类型安全 + 模块隔离 + i18n。~~

> ~~**AI 率 99.9%** — 本项目几乎全由Gemini Pro + DeepSeek生成。~~

---

## 项目定位

`tmoes` 是 [tmoe-linux](https://github.com/2moe/tmoe) 的一个**子集**，目标是在 Android [Droidspaces](https://github.com/nicholasgasior/droidspaces) 环境中运行。因此**不会 100% 移植 tmoe 的全部特性**，仅聚焦安卓容器场景的核心功能。

---

## 当前状态

**第一阶段 —— 初期**（尽量复刻 tmoe 的 UI 与核心功能）

| 阶段 | 内容                                         |
|---|--------------------------------------------|
| 一 | 复刻 tmoe 交互式菜单结构、容器管理、软件安装、镜像源切换            |
| 二 | 现代化工程化：更好更完整的I18n、进一步减少bash的占比、增强代码的抽象和封装性 |
| 三 | ~~咕咕咕 + 跑路~~                               |

**目前仅在 WSL2 Ubuntu 22.04 LTS 上测试过部分功能**，其他平台待验证。

---

## 构建

```bash
# 需要：CMake ≥ 3.16 / C++17 编译器 / Python 3（生成 i18n）

mkdir build && cd build
cmake ..
make -j$(nproc)
```

无硬依赖。纯 C++17 + STL + nlohmann_json 即可编译，通过 `popen`/`system` 调用系统命令。

---

## 架构

```
src/
├── core/          # 基础设施 (Executor / Config / Logger / I18n / CLI)
├── domain/
│   ├── apps/      # 应用管理器 (浏览器/终端/办公/输入法/下载工具)
│   ├── features/  # 秘密花园 (教育/实验功能)
│   ├── gui/       # 桌面环境/VNC/美化
│   ├── gui_config/# GUI 模板
│   ├── runtime/   # 容器运行时 (proot/chroot/nspawn)
│   ├── system/    # 系统工具 (镜像源/封装管理/备份/Termux)
│   └── virtualization/ # Docker + Wine
├── app/           # 应用编排 (Manager — 菜单路由与 UI 分流)
├── locales/       # 多语言 JSON (zh_CN / en_US)
└── data/          # 镜像源注册表 / rootfs 映射
```

## i18n

添加新语言只需在 `locales/` 下放一个 JSON：

```json
{ "key": "translation" }
```

运行时 `I18n::init("zh_CN")` 切换。编译时 Python 脚本将 JSON 嵌入二进制。

## 运行

```bash
tmoes            # 自动检测平台 (Termux→容器管理 / Linux→工具箱)
tmoes m          # 容器管理界面
tmoes t          # Linux 工具箱界面
```

## 许可证

与原 tmoe-linux 保持一致。
