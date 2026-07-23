#pragma once

#include "menu_item.h"
#include <functional>
#include <initializer_list>
#include <memory>

namespace tmoe::ui {

/**
 * 菜单容器基类。
 *
 * 实现 Composite 模式中的容器角色。一个 IUIMenu 可以包含
 * 任意数量的子菜单项（包括其他 IUIMenu 和 IAction），形成
 * 可任意嵌套的菜单树。
 *
 * 基类提供默认的 whiptail 命令构建实现，子类可通过重写
 * build_whiptail_cmd() 实现自定义渲染（如 --checklist）。
 */
    class IUIMenu : public IMenuItem {
    public:
        /** 菜单窗口标题（已翻译）。 */
        virtual std::string get_title() const = 0;

        /** 菜单提示文字（whiptail --menu 下方的说明文本）。 */
        virtual std::string get_prompt() const { return ""; }

        /** 构建 whiptail 命令字符串。
         *  默认实现遍历可见子项，生成 --menu 格式。
         *  子类可重写实现 --checklist、--radiolist 等。 */
        virtual std::string build_whiptail_cmd(const MenuContext &ctx) const;

        // ── 子项管理 ──

        /** 添加子菜单项（追加到末尾）。 */
        void add_child(std::shared_ptr<IMenuItem> item);

        /** 添加子菜单项到指定位置。 */
        void add_child_at(size_t index, std::shared_ptr<IMenuItem> item);

        /** 批量添加子菜单项（字面量语法：{a, b, c}）。 */
        void add_children(std::initializer_list<std::shared_ptr<IMenuItem>> items);

        /** 批量添加子菜单项（运行时容器：vector、list 等）。 */
        template<typename Container>
        void add_children(const Container &items) {
            for (auto &item: items) {
                children_.push_back(item);
            }
        }

        /** 返回所有子菜单项（包含不可见的项，子类决定是否过滤）。 */
        const std::vector<std::shared_ptr<IMenuItem>> &children() const { return children_; }

        /** 根据 tag 查找子项，找不到返回 nullptr。 */
        std::shared_ptr<IMenuItem> find_child(const std::string &tag) const;

        // ── 快捷构建方法 ──

        /** 快捷创建操作项。自动包装 LambdaAction，回调执行后自动 Logger::press_enter()。
         *  等价于 add_child(LambdaAction::make(label, tag, [fn]{ fn(); Logger::press_enter(); })) */
        void add_action(std::string label, std::string tag,
                        std::function<void()> fn);

        /** 快捷创建嵌套子菜单入口。点击后在子菜单中启动 MenuEngine。 */
        void add_submenu(std::string label, std::string tag,
                         std::shared_ptr<IUIMenu> submenu);

    protected:
        std::vector<std::shared_ptr<IMenuItem>> children_;
    };

} // namespace tmoe::ui
