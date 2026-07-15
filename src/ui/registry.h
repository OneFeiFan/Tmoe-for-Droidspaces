#pragma once
#include "menu_item.h"
#include <vector>
#include <memory>
#include <mutex>

namespace tmoe::ui {

/**
 * 全局菜单项注册表。
 *
 * 各领域模块在初始化时调用 register_item() 注册自己的
 * 菜单入口（IUIMenu 或 IAction），主菜单组装时从注册表
 * 收集所有已注册项。
 *
 * 线程安全：register_item 使用互斥锁保护。
 */
class MenuRegistry {
public:
    /** 注册一个菜单项。通常在模块构造或静态初始化时调用。 */
    static void register_item(std::shared_ptr<IMenuItem> item);

    /** 返回所有已注册菜单项的拷贝（用于构建主菜单）。 */
    static std::vector<std::shared_ptr<IMenuItem>> items();

    /** 清空注册表（主要用于测试）。 */
    static void clear();

private:
    static std::vector<std::shared_ptr<IMenuItem>>& registry();
    static std::mutex& mutex();
};

} // namespace tmoe::ui
