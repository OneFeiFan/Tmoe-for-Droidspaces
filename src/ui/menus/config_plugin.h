#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class ConfigManager; }

namespace tmoe::ui::menus {

class ConfigMenuPlugin : public PluginFor<domain::ConfigManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:
    std::shared_ptr<IUIMenu> build_dns_menu();
    std::shared_ptr<IUIMenu> build_timezone_region_menu();
    std::shared_ptr<IUIMenu> build_timezone_city_menu(int region_index);
    std::shared_ptr<IUIMenu> build_locale_region_menu();
    std::shared_ptr<IUIMenu> build_locale_select_menu(int region_index);
};

} // namespace tmoe::ui::menus
