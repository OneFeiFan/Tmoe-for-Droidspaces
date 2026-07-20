#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class BackupManager; }

namespace tmoe::ui::menus {

/** 备份菜单插件。
 *  构建 9 个菜单项：备份/恢复容器、清理垃圾、外置存储备份、列表、空间排行、Timeshift、Termux 宿主备份、备份目录大小。 */
class BackupMenuPlugin : public PluginFor<domain::BackupManager> {
public:
    using PluginFor::PluginFor;
    std::shared_ptr<IUIMenu> build() override;
};

} // namespace tmoe::ui::menus
