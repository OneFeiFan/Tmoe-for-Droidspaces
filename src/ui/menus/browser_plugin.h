#pragma once

#include "ui/plugin.h"

namespace tmoe::domain { class BrowserManager; }

namespace tmoe::ui::menus {

/** 浏览器安装菜单插件。
 *  构建 6 个菜单项：Firefox/Chromium / Edge / Falkon / Vivaldi / Epiphany / Midori。 */
    class BrowserMenuPlugin : public PluginFor<domain::BrowserManager> {
    public:
        using PluginFor::PluginFor;

        std::shared_ptr<IUIMenu> build() override;
    };

} // namespace tmoe::ui::menus
