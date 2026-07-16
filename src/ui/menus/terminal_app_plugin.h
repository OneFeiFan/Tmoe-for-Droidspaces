#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class TerminalAppManager; }

namespace tmoe::ui::menus {

/** 终端应用安装菜单插件。
 *  构建 17 个终端模拟器菜单项。 */
class TerminalAppMenuPlugin : public IPlugin {
public:
    explicit TerminalAppMenuPlugin(domain::TerminalAppManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::TerminalAppManager* mgr_;
};

} // namespace tmoe::ui::menus
