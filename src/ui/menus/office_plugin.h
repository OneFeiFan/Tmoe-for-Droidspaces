#pragma once

#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class OfficeManager; }

namespace tmoe::ui::menus {

/** 办公软件安装菜单插件。
 *  构建菜单项：LibreOffice / LibreOffice(中文) / WPS / Yozo / FreeOffice / Meld / KDiff3 / Manpages(中文)。 */
    class OfficeMenuPlugin : public PluginFor<domain::OfficeManager> {
    public:
        using PluginFor::PluginFor;

        std::shared_ptr<IUIMenu> build() override;
    };

} // namespace tmoe::ui::menus
