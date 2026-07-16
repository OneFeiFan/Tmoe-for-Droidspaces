/** Backup 菜单插件 — 包装 BackupManager::run_backup_menu()。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/backup.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_backup_menu(domain::BackupManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.backup"), _("backup.menu_prompt"), "plugin_backup");

    menu->add_child(std::make_shared<LambdaAction>(
        _("menu.tui.backup"), "enter_backup",
        [mgr](MenuContext&) -> bool {
            mgr->run_backup_menu();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
