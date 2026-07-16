#pragma once
#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class DockerManager; }

namespace tmoe::ui::menus {

/** Docker 菜单插件。
 *  7 个直接操作项：拉取镜像 / Portainer / 导出 / 导入 / 镜像源 / 安装 CE / 添加用户。 */
class DockerMenuPlugin : public IPlugin {
public:
    explicit DockerMenuPlugin(domain::DockerManager* mgr);
    std::shared_ptr<IUIMenu> build() override;

private:
    domain::DockerManager* mgr_;
};

} // namespace tmoe::ui::menus
