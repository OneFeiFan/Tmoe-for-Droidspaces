#pragma once
#include <functional>
#include <map>
#include "core/logger.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/config.h"
#include "domain/container.h"
#include "domain/environment.h"
#include "domain/gui.h"
#include "domain/mirrors.h"
#include "domain/mirror_registry.h"
#include "domain/backup.h"
#include "domain/virtualization.h"
#include "domain/docker.h"
#include "domain/termux.h"
#include <memory>

#include "core/launch_context.h"
#include "domain/container_manager.h"

namespace tmoe::app {
    /** 顶层应用管理器。
     *  编排各领域模块、TUI 菜单路由和命令分发。
     */
    class Manager {
    public:
        explicit Manager(TmoeConfig cfg);

        /** 运行交互式 TUI 循环。 */
        int run_interactive();

        /** 根据预解析的 LaunchContext 执行对应操作。 */
        int run_launch_context(const LaunchContext &ctx);

    private:
        TmoeConfig cfg_;

        // 领域模块
        std::unique_ptr<domain::ContainerManager> container_mgr_;
        std::unique_ptr<domain::Environment> environment_;
        std::unique_ptr<domain::TermuxManager> termux_;
        std::unique_ptr<domain::GUIManager> gui_;
        std::unique_ptr<domain::MirrorManager> mirror_mgr_;
        std::unique_ptr<domain::BackupManager> backup_mgr_;
        std::unique_ptr<domain::DockerManager> docker_mgr_;
        std::unique_ptr<domain::VirtualizationManager> virt_mgr_;

        // TUI 命令路由表
        std::map<std::string, std::function<void()> > tui_routes_;

        /** 初始化 tui_routes_ 中的菜单处理函数。 */
        void init_routes();

        /** 镜像源管理子菜单。 */
        void run_mirror_menu();

        /** 语言/区域切换菜单（支持全部 7 种语言）。 */
        void run_locale_menu();

        /** 渲染 whiptail 菜单并返回用户选择的标签。 */
        std::string render_and_get_choice();
    };
} // namespace tmoe::app
