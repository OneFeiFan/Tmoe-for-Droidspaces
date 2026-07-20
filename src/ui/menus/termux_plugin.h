#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class TermuxManager; }

namespace tmoe::ui::menus {

class TermuxMenuPlugin : public PluginFor<domain::TermuxManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:

    std::shared_ptr<IUIMenu> build_termux_gui_menu();
    std::shared_ptr<IUIMenu> build_termux_beautify_menu();
    std::shared_ptr<IUIMenu> build_repos_menu();
    std::shared_ptr<IUIMenu> build_alpine_mirror_menu();
    std::shared_ptr<IUIMenu> build_mirror_menu();
    std::shared_ptr<IUIMenu> build_disk_menu();
    std::shared_ptr<IUIMenu> build_adb_connect_menu();
    std::shared_ptr<IUIMenu> build_signal9_menu();
    std::shared_ptr<IUIMenu> build_color_menu();
    std::shared_ptr<IUIMenu> build_font_menu();
};

} // namespace tmoe::ui::menus
