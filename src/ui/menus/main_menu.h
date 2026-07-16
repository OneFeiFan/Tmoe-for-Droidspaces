#pragma once
#include "ui/menu.h"
#include <string>

namespace tmoe::ui::menus {

/**
 * 主菜单构建器。
 *
 * 从 MenuRegistry 收集所有已注册插件，构建插件化主菜单。
 * 新增插件只需注册到 MenuRegistry，无需修改此处。
 */
class MenuBuilder {
public:
    /** 从 MenuRegistry 收集所有已注册插件，构建主菜单。 */
    static std::shared_ptr<IUIMenu> build_from_registry(
        const std::string& title);
};

} // namespace tmoe::ui::menus
