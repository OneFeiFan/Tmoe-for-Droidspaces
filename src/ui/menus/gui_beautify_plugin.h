#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class GUIManager; }

namespace tmoe::ui::menus {

/** 桌面美化菜单插件。
 *  6 个独立操作：主题 / 图标 / 壁纸 / 鼠标光标 / 停靠栏 / Compiz。 */
class BeautifyMenuPlugin : public IPlugin {
public:
    explicit BeautifyMenuPlugin(domain::GUIManager* gui);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::GUIManager* gui_;
};

} // namespace tmoe::ui::menus
