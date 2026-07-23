#pragma once

#include "menu_item.h"
#include "menu.h"
#include <memory>
#include <vector>

namespace tmoe::ui {

/**
 * 菜单导航引擎。
 *
 * 职责：
 * - 渲染当前菜单 (IUIMenu::build_whiptail_cmd → Executor::tui_select)
 * - 根据用户选择分发：进入子菜单 / 执行 IAction / 返回上级
 * - 维护导航历史栈，支持任意深度嵌套和回退
 *
 * 使用示例：
 *   MenuEngine engine(MenuContext{cfg});
 *   return engine.run(main_menu);
 */
    class MenuEngine {
    public:
        explicit MenuEngine(MenuContext ctx);

        /** 从根菜单启动交互式 TUI 循环。
         *  返回 0 表示正常退出，非 0 表示异常。 */
        int run(std::shared_ptr<IUIMenu> root_menu);

    private:
        MenuContext ctx_;
        /** 导航历史栈（不含当前菜单）。
         *  back() 是上级菜单，空栈表示当前是根菜单。 */
        std::vector<std::shared_ptr<IUIMenu>> history_;

        /** 渲染当前菜单并获取用户选择。 */
        std::string render(const IUIMenu &menu);

        /** 分发用户选择：
         *  - 子项是 IUIMenu → 入栈并切换
         *  - 子项是 IAction → 执行，返回上级
         *  - tag 为 "back" → 出栈返回上级
         *  - tag 为 "exit" → 退出循环
         *  返回 true 表示继续循环，false 表示退出。 */
        bool dispatch(const std::string &choice, std::shared_ptr<IUIMenu> &current_menu);
    };

} // namespace tmoe::ui
