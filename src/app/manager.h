#pragma once
#include <functional>
#include <map>

#include "core/config.h"
#include "domain/container.h"
#include "domain/environment.h"
#include "domain/gui.h"
#include "domain/mirrors.h"
#include "domain/mirror_registry.h"
#include "domain/backup.h"
#include "domain/virtualization.h"
#include "domain/termux.h"
#include <memory>

#include "core/launch_context.h"
#include "domain/container_manager.h"

namespace tmoe::app {
    class Manager {
    public:
        explicit Manager(TmoeConfig cfg);

        int run_interactive();

        int run_launch_context(const LaunchContext &ctx);

    private:
        TmoeConfig cfg_;

        // 领域模块
        std::unique_ptr<domain::ContainerManager> container_mgr_;
        std::unique_ptr<domain::Environment> environment_;
        std::unique_ptr<domain::TermuxManager> termux_;

        // ── 新增：GUI 与多媒体生命周期管理模块 ──
        std::unique_ptr<domain::GUIManager> gui_;

        // ── 新增：镜像源切换管理模块 ──
        std::unique_ptr<domain::MirrorManager> mirror_mgr_;

        // ── 核心创新点：TUI 命令路由表 ──
        std::map<std::string, std::function<void()> > tui_routes_;

        // 初始化路由映射
        void init_routes();

        // 镜像源子菜单
        void run_mirror_menu();

        // 抽取 UI 渲染逻辑
        std::string render_and_get_choice();
    };
} // namespace tmoe::app
