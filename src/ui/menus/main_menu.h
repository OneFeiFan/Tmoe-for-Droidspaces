#pragma once
#include "ui/menu.h"
#include "ui/menu_engine.h"
#include "ui/action.h"
#include <functional>
#include <map>

namespace tmoe::app { class Manager; }

namespace tmoe::ui::menus {

/**
 * 主菜单构建器。
 *
 * Phase 2 迁移期：将旧的 tui_routes_ 路由表包装为
 * IUIMenu 树，通过 MenuEngine 驱动，与原路径并行运行。
 */
class MenuBuilder {
public:
    using RouteMap = std::map<std::string, std::function<void()>>;

    /**
     * 构建完整的"容器管理"菜单树 (18 项 → 对应旧 render_and_get_choice)。
     * @param title    菜单标题
     * @param routes   旧的 tui_routes_ 映射表
     * @param create_submenus  子菜单工厂函数（label → sub_menu）
     */
    static std::shared_ptr<IUIMenu> build_main_menu(
        const std::string& title,
        const RouteMap& routes);

    /**
     * 构建"Linux 工具箱"菜单 (9 项 → 对应旧 render_tool_menu)。
     * 内部重新映射选项编号到 routes key。
     */
    static std::shared_ptr<IUIMenu> build_tool_menu(
        const std::string& title,
        const RouteMap& routes);

    /**
     * 构建"容器管理"菜单 (10 项 → 对应旧 render_manager_menu)。
     */
    static std::shared_ptr<IUIMenu> build_manager_menu(
        const std::string& title,
        const RouteMap& routes);

    /**
     * 从 MenuRegistry 收集所有已注册插件，构建插件化主菜单。
     * 这是插件架构的最终目标——新增插件只需注册，无需修改主菜单。
     */
    static std::shared_ptr<IUIMenu> build_from_registry(
        const std::string& title);

private:
    /** 为 routes 中的一系列 key→label 映射创建 DelegateAction 列表。 */
    static std::vector<std::shared_ptr<IMenuItem>> make_items(
        const RouteMap& routes,
        const std::vector<std::pair<std::string, std::string>>& key_labels);
};

} // namespace tmoe::ui::menus
