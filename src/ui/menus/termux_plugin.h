#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class TermuxManager; }

namespace tmoe::ui::menus {

class TermuxMenuPlugin : public IPlugin {
public:
    explicit TermuxMenuPlugin(domain::TermuxManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::TermuxManager* mgr_;

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
