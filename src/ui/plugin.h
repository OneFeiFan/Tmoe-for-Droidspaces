#pragma once
#include "menu.h"
#include <memory>

namespace tmoe::ui {

/**
 * UI 插件抽象基类。
 *
 * 每个领域模块对应一个插件类（如 DesktopMenuPlugin、BrowserMenuPlugin），
 * 负责构建该模块的 IUIMenu 菜单树。
 *
 * 使用方式：
 *   MenuEngine(ctx).run(DesktopMenuPlugin(gui).build());
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /** 构建并返回完整的菜单树（含导航项）。 */
    virtual std::shared_ptr<IUIMenu> build() = 0;
};

} // namespace tmoe::ui
