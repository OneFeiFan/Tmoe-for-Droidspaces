#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class BrowserManager; }

namespace tmoe::ui::menus {

/** 浏览器安装菜单插件。
 *  构建 6 个菜单项：Firefox/Chromium / Edge / Falkon / Vivaldi / Epiphany / Midori。 */
class BrowserMenuPlugin : public IPlugin {
public:
    explicit BrowserMenuPlugin(domain::BrowserManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::BrowserManager* mgr_;
};

} // namespace tmoe::ui::menus
