#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class EducationManager; }

namespace tmoe::ui::menus {

class EducationMenuPlugin : public PluginFor<domain::EducationManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:

    std::shared_ptr<IUIMenu> build_gaokao_menu();
    std::shared_ptr<IUIMenu> build_gaokao_papers_menu();
    std::shared_ptr<IUIMenu> build_gaokao_notes_menu();
    std::shared_ptr<IUIMenu> build_kaoyan_menu();
    std::shared_ptr<IUIMenu> build_math_menu();
    std::shared_ptr<IUIMenu> build_english_menu();
    std::shared_ptr<IUIMenu> build_goldendict_menu();
    std::shared_ptr<IUIMenu> build_cet_menu();
    std::shared_ptr<IUIMenu> build_physics_menu();
    std::shared_ptr<IUIMenu> build_chemistry_menu();
};

} // namespace tmoe::ui::menus
