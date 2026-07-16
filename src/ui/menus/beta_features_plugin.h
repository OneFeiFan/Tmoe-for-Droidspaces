#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class BetaFeaturesManager; }

namespace tmoe::ui::menus {

class BetaFeaturesMenuPlugin : public IPlugin {
public:
    explicit BetaFeaturesMenuPlugin(domain::BetaFeaturesManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::BetaFeaturesManager* mgr_;

    std::shared_ptr<IUIMenu> build_system_menu();
    std::shared_ptr<IUIMenu> build_uefi_menu();
    std::shared_ptr<IUIMenu> build_store_menu();
    std::shared_ptr<IUIMenu> build_deepin_menu();
    std::shared_ptr<IUIMenu> build_video_menu();
    std::shared_ptr<IUIMenu> build_paint_menu();
    std::shared_ptr<IUIMenu> build_r_lang_menu();
    std::shared_ptr<IUIMenu> build_file_menu();
    std::shared_ptr<IUIMenu> build_file_manager_menu();
    std::shared_ptr<IUIMenu> build_reader_menu();
    std::shared_ptr<IUIMenu> build_other_menu();
    std::shared_ptr<IUIMenu> build_scrcpy_menu();
};

} // namespace tmoe::ui::menus
