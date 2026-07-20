#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class InputMethodManager; }

namespace tmoe::ui::menus {

/** 输入法菜单插件。
 *  3 个嵌套子菜单：fcitx4 / fcitx5 / ibus，加上 Sogou 和 FAQ 直接操作。 */
class InputMethodMenuPlugin : public PluginFor<domain::InputMethodManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;

private:

    std::shared_ptr<IUIMenu> build_fcitx4_menu();
    std::shared_ptr<IUIMenu> build_fcitx5_menu();
    std::shared_ptr<IUIMenu> build_ibus_menu();
};

} // namespace tmoe::ui::menus
