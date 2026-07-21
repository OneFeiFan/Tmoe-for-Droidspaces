#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class SoftwareCenter; }

namespace tmoe::ui::menus {

class SoftwareCenterMenuPlugin : public PluginFor<domain::SoftwareCenter> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:

    std::shared_ptr<IUIMenu> build_image_compression_menu();
    std::shared_ptr<IUIMenu> build_media_menu();
    std::shared_ptr<IUIMenu> build_sns_menu();
    std::shared_ptr<IUIMenu> build_games_menu();
    std::shared_ptr<IUIMenu> build_pkg_gui_menu();
    std::shared_ptr<IUIMenu> build_fileshare_menu();
    std::shared_ptr<IUIMenu> build_cleanup_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_music_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_note_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_video_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_picture_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_reader_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_game_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_development_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_tools_menu();
    std::shared_ptr<IUIMenu> build_debian_opt_internet_menu();
    std::shared_ptr<IUIMenu> build_electron_vm_menu();
    std::shared_ptr<IUIMenu> build_electron_music_menu();
    std::shared_ptr<IUIMenu> build_electron_video_menu();
    std::shared_ptr<IUIMenu> build_electron_note_menu();
    std::shared_ptr<IUIMenu> build_electron_dev_menu();
    std::shared_ptr<IUIMenu> build_electron_manager_menu();
};

} // namespace tmoe::ui::menus
