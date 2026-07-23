#include "menu_engine.h"
#include "action.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"

namespace tmoe::ui {

    MenuEngine::MenuEngine(MenuContext ctx) : ctx_(std::move(ctx)) {}

    int MenuEngine::run(std::shared_ptr<IUIMenu> root_menu) {
        auto current = std::move(root_menu);
        history_.clear();

        while (current) {
            std::string choice = render(*current);

            // 空选择或取消 → 返回上级，根菜单则退出
            if (choice.empty()) {
                if (history_.empty()) break;
                current = history_.back();
                history_.pop_back();
                continue;
            }

            if (!dispatch(choice, current)) {
                break; // exit 操作
            }
        }

        return 0;
    }

    std::string MenuEngine::render(const IUIMenu &menu) {
        std::string cmd = menu.build_whiptail_cmd(ctx_);
        Logger::debug("MenuEngine render: " + cmd);
        return Executor::tui_select(cmd);
    }

    bool MenuEngine::dispatch(const std::string &choice, std::shared_ptr<IUIMenu> &current_menu) {
        // 内置导航操作
        if (choice == "exit") {
            return false; // 退出引擎
        }
        if (choice == "back") {
            if (history_.empty()) return false; // 根菜单无上级
            current_menu = history_.back();
            history_.pop_back();
            return true; // 继续循环
        }

        // 查找子项
        auto child = current_menu->find_child(choice);
        if (!child) {
            Logger::warn(std::string(_("menu.unimplemented")) + ": " + choice);
            Logger::press_enter();
            return true; // 继续循环
        }

        // 分发：容器 → 进入子菜单；叶子 → 执行操作
        if (auto sub_menu = std::dynamic_pointer_cast<IUIMenu>(child)) {
            history_.push_back(current_menu);
            current_menu = sub_menu;
            return true;
        }

        if (auto action = std::dynamic_pointer_cast<IAction>(child)) {
            bool ok = action->execute(ctx_);
            if (!ok) {
                Logger::warn(_("app.operation_failed"));
                Logger::press_enter();
            }
            // 执行后返回上级菜单
            if (!history_.empty()) {
                current_menu = history_.back();
                history_.pop_back();
            }
            return true;
        }

        // 未知类型
        Logger::warn(std::string(_("menu.unimplemented")) + ": " + choice);
        Logger::press_enter();
        return true;
    }

} // namespace tmoe::ui
