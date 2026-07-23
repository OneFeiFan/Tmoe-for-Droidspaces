/** UI 插件工厂 — 通过模板消除 17 个重复工厂函数。 */
#pragma once

#include "ui/menu.h"
#include "ui/menus/gui_desktop_plugin.h"
#include "ui/menus/gui_beautify_plugin.h"
#include "ui/menus/gui_remote_plugin.h"
#include "ui/menus/browser_plugin.h"
#include "ui/menus/office_plugin.h"
#include "ui/menus/terminal_app_plugin.h"
#include "ui/menus/download_tools_plugin.h"
#include "ui/menus/input_method_plugin.h"
#include "ui/menus/dev_tools_plugin.h"
#include "ui/menus/docker_plugin.h"
#include "ui/menus/backup_plugin.h"
#include "ui/menus/virtualization_plugin.h"
#include "ui/menus/education_plugin.h"
#include "ui/menus/config_plugin.h"
#include "ui/menus/termux_plugin.h"
#include "ui/menus/software_center_plugin.h"
#include "ui/menus/beta_features_plugin.h"
#include <memory>

namespace tmoe::ui::menus {

/** 通用工厂：实例化 Plugin 并调用 build()。 */
    template<typename Plugin, typename Mgr>
    inline std::shared_ptr<IUIMenu> make_menu(Mgr *mgr) {
        return Plugin(mgr).build();
    }

} // namespace tmoe::ui::menus
