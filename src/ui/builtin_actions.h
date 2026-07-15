#pragma once
#include "action.h"
#include "menu.h"
#include "core/i18n.h"

namespace tmoe::ui {

// ── 内置操作 ──────────────────────────────────────────

/** 返回上级菜单。Engine 通过 get_tag() == "back" 识别。 */
class BackAction : public IAction {
public:
    std::string get_label() const override { return _("menu.tui.back_upper"); }
    std::string get_tag() const override { return "back"; }
    bool execute(MenuContext&) override { return true; } // Engine 拦截，不会到达此处
    bool needs_root() const override { return false; }
};

/** 退出程序。Engine 通过 get_tag() == "exit" 识别。 */
class ExitAction : public IAction {
public:
    std::string get_label() const override { return _("menu.tui.exit"); }
    std::string get_tag() const override { return "exit"; }
    bool execute(MenuContext&) override { return true; } // Engine 拦截，不会到达此处
    bool needs_root() const override { return false; }
};

// ── 辅助方法 ──────────────────────────────────────────

/** 为 sub_menu 自动添加"返回"和"退出"选项。
 *  内部调用 sub_menu->add_children({BackAction, ExitAction})。 */
inline void add_navigation_items(std::shared_ptr<IUIMenu> sub_menu) {
    sub_menu->add_child(std::make_shared<BackAction>());
    sub_menu->add_child(std::make_shared<ExitAction>());
}

/** 创建一个带标准导航选项的子菜单。 */
inline std::shared_ptr<IUIMenu> make_menu_with_nav(std::shared_ptr<IUIMenu> menu) {
    add_navigation_items(menu);
    return menu;
}

// ── 简单容器菜单 ──────────────────────────────────────

/** 具体的 IUIMenu 实现——通过构造函数注入 title/label/tag，
 *  使用基类默认的 build_whiptail_cmd() 遍历 children。 */
class SimpleMenu : public IUIMenu {
public:
    SimpleMenu(std::string title, std::string label, std::string tag)
        : title_(std::move(title)), label_(std::move(label)), tag_(std::move(tag)) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    std::string get_title() const override { return title_; }
    std::string get_prompt() const override { return _("menu.tui.title"); }

private:
    std::string title_, label_, tag_;
};

} // namespace tmoe::ui
