#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class ConfigManager; }

namespace tmoe::ui::menus {

class ConfigMenuPlugin : public IPlugin {
public:
    explicit ConfigMenuPlugin(domain::ConfigManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::ConfigManager* mgr_;

    std::shared_ptr<IUIMenu> build_dns_menu();
    std::shared_ptr<IUIMenu> build_timezone_region_menu();
    std::shared_ptr<IUIMenu> build_timezone_city_menu(int region_index);
    std::shared_ptr<IUIMenu> build_locale_region_menu();
    std::shared_ptr<IUIMenu> build_locale_select_menu(int region_index);
    std::shared_ptr<IUIMenu> build_fortune_menu();
};

} // namespace tmoe::ui::menus
