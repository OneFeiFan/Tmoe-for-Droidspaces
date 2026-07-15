---
name: ui-plugin
description: 使用 TMOE UI 插件框架创建新菜单/功能模块——基于 IMenuItem/IAction/IUIMenu 的 Composite 模式 TUI 架构
---

## 概述

为 tmoes 创建新的 TUI 菜单或功能模块时，使用 `src/ui/` 下的插件化框架，替代旧式字符串拼接 whiptail 命令的方式。

核心三元素：

```
IMenuItem (抽象基类)         ← 菜单项的共同属性：标签、标识符、可见性
  ├── IAction (叶子)         ← 用户选择后直接执行操作
  └── IUIMenu (容器)         ← 包含子项（可嵌套），形成菜单树
```

`MenuEngine` 驱动渲染→选择→分发→导航的循环，`MenuRegistry` 管理全局插件注册。

## 快速开始

### 新建一个叶子操作（IAction）

```cpp
// src/ui/menus/my_feature.cpp
#include "ui/plugin_helpers.h"

namespace tmoe::ui::menus {

class InstallMyAppAction : public IAction {
public:
    std::string get_label() const override { return _("myapp.install_label"); }
    std::string get_tag() const override { return "inst_myapp"; }

    bool execute(MenuContext& ctx) override {
        auto family = domain::PackageManager::detect_distro_family();
        bool ok = domain::PackageManager::install("myapp", family);
        if (ok) Logger::ok(_("app.install_done"));
        else    Logger::error(_("app.install_failed"));
        Logger::press_enter();
        return ok;
    }

    bool needs_root() const override { return false; }
};

// 注册到全局注册表
static AutoRegister reg_myapp(std::make_shared<InstallMyAppAction>());

} // namespace tmoe::ui::menus
```

### 新建一个子菜单（IUIMenu）

```cpp
#include "ui/plugin_helpers.h"

namespace tmoe::ui::menus {

class BrowserMenu : public SimpleMenu {
public:
    BrowserMenu()
        : SimpleMenu(_("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser")
    {
        // 1. 内容项在前
        add_child(make_install_action("Firefox", "inst_ff", []() {
            return domain::PackageManager::install("firefox",
                domain::PackageManager::detect_distro_family());
        }));
        add_child(make_install_action("Chromium", "inst_ch", []() {
            return domain::PackageManager::install("chromium",
                domain::PackageManager::detect_distro_family());
        }));

        // 2. 导航最后加（夹心饼结构）
        add_sandwich_nav(std::shared_ptr<IUIMenu>(this, [](IUIMenu*){}));
        // 注意：构造函数内 this 不是 shared_ptr。正确做法见下方"注册"部分。
    }
};

} // namespace tmoe::ui::menus
```

> **构造函数内不能用 `shared_from_this()`。** 正确的模式是在构造函数中只加内容项，在外部工厂函数中加导航。

### 正确的注册模式

```cpp
// 工厂函数：创建菜单 → 加内容 → 加夹心饼导航 → 注册
static void register_browser_menu() {
    auto menu = make_plugin_menu(
        _("browser.menu_title"), _("browser.menu_prompt"), "plugin_browser");

    menu->add_child(make_install_action("Firefox", "inst_ff", []() {
        return domain::PackageManager::install("firefox",
            domain::PackageManager::detect_distro_family());
    }));

    add_sandwich_nav(menu);  // 所有内容加完后调用
    MenuRegistry::register_item(menu);
}

// 在 Manager::run_interactive_plugin() 中调用
register_browser_menu();
```

## API 参考

### IMenuItem — `src/ui/menu_item.h`

| 方法 | 返回 | 说明 |
|------|------|------|
| `get_label()` | `string` | whiptail 菜单中显示的文字（已翻译） |
| `get_tag()` | `string` | 唯一标识符，用于路由分发。不可重复 |
| `is_visible(ctx)` | `bool` | 条件显示。默认返回 true |

### IAction : IMenuItem — `src/ui/action.h`

| 方法 | 返回 | 说明 |
|------|------|------|
| `execute(ctx)` | `bool` | 执行操作，返回 true 表示成功 |
| `needs_root()` | `bool` | 是否需要 root。默认 true |

### IUIMenu : IMenuItem — `src/ui/menu.h`

| 方法 | 返回 | 说明 |
|------|------|------|
| `get_title()` | `string` | 菜单窗口标题 |
| `get_prompt()` | `string` | whiptail 提示文字。默认 `_("menu.tui.title")` |
| `build_whiptail_cmd(ctx)` | `string` | 构建 whiptail 命令。默认遍历可见子项生成 `--menu` |
| `add_child(item)` | `void` | 追加子项到末尾 |
| `add_child_at(idx, item)` | `void` | 插入子项到指定位置 |
| `add_children({...})` | `void` | 批量追加（initializer_list） |
| `add_children(vec)` | `void` | 批量追加（vector 等容器） |
| `find_child(tag)` | `shared_ptr` | 按 tag 查找子项 |

### MenuContext — `src/ui/menu_item.h`

```cpp
struct MenuContext {
    TmoeConfig& cfg;  // 全局配置
};
```

### LambdaAction — `src/ui/plugin_helpers.h`

快速创建无需自定义类的 `IAction`：

```cpp
// 简单回调
auto action = LambdaAction::make("标签", "tag", []() { do_something(); });

// 需要 MenuContext
auto action = std::make_shared<LambdaAction>("标签", "tag",
    [](MenuContext& ctx) -> bool {
        Logger::info(ctx.cfg.os_pretty_name);
        return true;
    });
```

### 快捷工厂方法 — `src/ui/plugin_helpers.h`

| 工厂 | 用途 |
|------|------|
| `make_plugin_menu(title, label, tag)` | 创建空的 SimpleMenu |
| `make_install_action(label, tag, fn)` | 创建"安装→确认→Logger输出"模式的 LambdaAction |
| `add_navigation_items(menu)` | 底部追加 返回+退出（平板结构） |
| `add_sandwich_nav(menu)` | 顶部插入返回 + 底部追加退出（夹心饼结构） |

### SimpleMenu — `src/ui/builtin_actions.h`

具体的 `IUIMenu` 实现——构造函数注入 title/label/tag，使用默认 `build_whiptail_cmd()`。

### MenuEngine — `src/ui/menu_engine.h`

| 方法 | 说明 |
|------|------|
| `run(root_menu)` | 启动交互循环。自动处理渲染/选择/分发/导航 |

引擎自动识别子项类型：
- `IUIMenu` → 入栈，进入子菜单
- `IAction` → 执行，返回上级
- `tag == "back"` → 出栈，返回上级
- `tag == "exit"` → 退出引擎

### MenuRegistry — `src/ui/registry.h`

| 静态方法 | 说明 |
|----------|------|
| `register_item(item)` | 注册菜单项（线程安全） |
| `items()` | 返回所有已注册项的拷贝 |
| `clear()` | 清空（测试用） |

## 导航模式

### 夹心饼 `add_sandwich_nav(menu)`

用于叶子菜单（子项全是 IAction）：

```
← 返回上级菜单          ← 顶部（快捷回退，tag="back"）
─────────────────
Firefox              ← 内容
Chromium
─────────────────
🌚 退出                ← 底部（tag="exit"）
```

### 平板 `add_navigation_items(menu)`

用于根菜单或内容已自包含的菜单：

```
Container Management  ← 内容（可能是 IUIMenu）
GUI Desktop
Software Center
─────────────────
← 返回上级菜单          ← 底部
🌚 退出
```

**规则：** 含 `IAction` 叶子项的子菜单用夹心饼，含 `IUIMenu` 容器的菜单用平板。

## i18n 集成

所有用户可见文本必须通过 `_(...)` 翻译：

```cpp
get_label() { return _("myapp.install_label"); }
get_title() { return _("myapp.menu_title"); }
```

Key 命名规范：`模块名.功能描述`

## 迁移指南（从旧式 whiptail 代码）

### 旧式代码

```cpp
void SomeClass::run_my_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"Title\" --menu \"Prompt\" 0 0 0 "
            "\"1\" \"Option A\" "
            "\"2\" \"Option B\" "
            "\"0\" \"Back\"";
        std::string ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;
        if (ch == "1") { do_a(); }
        else if (ch == "2") { do_b(); }
    }
}
```

### 新式代码

```cpp
void SomeClass::register_my_menu() {
    auto menu = make_plugin_menu(
        _("mymod.title"), _("mymod.prompt"), "my_menu");

    menu->add_child(make_install_action(
        _("mymod.option_a"), "opt_a", [this]() { return do_a(); }));
    menu->add_child(make_install_action(
        _("mymod.option_b"), "opt_b", [this]() { return do_b(); }));

    add_sandwich_nav(menu);
    MenuRegistry::register_item(menu);
}
```

**变化：**
- `while(true)` 循环 → `MenuEngine` 自动处理
- 字符串拼接 → `SimpleMenu::build_whiptail_cmd()` 自动生成
- `if/else` 分发 → `IAction::execute()` 多态分发
- 硬编码标签 → `_(...)` i18n 宏

## 反模式

❌ **直接在构造函数中用 `this` 调 `add_sandwich_nav`** — 此时 shared_ptr 不存在
```cpp
// 错误
class BadMenu : public SimpleMenu {
    BadMenu() {
        add_child(...);
        add_sandwich_nav(???);  // this 不是 shared_ptr!
    }
};
```

✅ **用外部工厂函数**
```cpp
void register_menu() {
    auto menu = make_plugin_menu(...);
    menu->add_child(...);
    add_sandwich_nav(menu);
    MenuRegistry::register_item(menu);
}
```

❌ **忘记在 `manager.cpp` 的 `run_interactive_plugin()` 中显式注册** — 静态库会丢弃 `AutoRegister`
```cpp
// AutoRegister 在静态库中不生效！必须显式调用注册函数。
```

✅ **在 `run_interactive_plugin()` 中显式调用注册函数**
```cpp
int Manager::run_interactive_plugin() {
    // 显式注册所有插件
    register_browser_menu();
    register_editor_menu();
    // ...

    auto root = MenuBuilder::build_from_registry(title);
    MenuEngine engine(ctx);
    return engine.run(root);
}
```

❌ **子菜单嵌套过深** — whiptail 不适合超过 3 层
❌ **用 `add_navigation_items` 代替 `add_sandwich_nav` 给叶子菜单** — 导航项不在顶部，用户体验差
❌ **tag 重复** — 导致 `find_child` 只返回第一个匹配项
