#pragma once
#include <functional>
#include <map>
#include "core/logger.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/config.h"
#include "domain/runtime/container.h"
#include "domain/system/environment.h"
#include "../domain/gui/gui.hpp"
#include "domain/system/mirrors.h"
#include "domain/system/mirror_registry.h"
#include "domain/system/backup.h"
#include "domain/virtualization/virtualization.h"
#include "domain/virtualization/docker.h"
#include "domain/system/termux.h"
#include "domain/system/config_manager.h"
#include "domain/apps/software_center.h"
#include "domain/apps/terminal_app.h"
#include "domain/apps/office.h"
#include "domain/features/education.h"
#include "domain/apps/input_method.h"
#include "domain/apps/browser.h"
#include "domain/features/beta_features.h"
#include "domain/apps/dev_tools.h"
#include "domain/apps/download_tools.h"
#include <memory>

namespace tmoe::ui {
    class IUIMenu;
}

#include "core/launch_context.h"
#include "domain/runtime/container_manager.h"

namespace tmoe::app {
    /** 顶层应用管理器。
     *  编排各领域模块、TUI 菜单路由和命令分发。
     */
    class Manager {
    public:
        using LocaleSaveFunc = std::function<void(std::string_view)>;

        explicit Manager(TmoeConfig cfg,
                         LocaleSaveFunc save_locale = nullptr);

        /** 运行交互式 TUI 循环（插件版：MenuRegistry 驱动）。 */
        int run_interactive_plugin();

        /** 根据预解析的 LaunchContext 执行对应操作。 */
        int run_launch_context(const LaunchContext &ctx);

    private:
        TmoeConfig cfg_;
        LocaleSaveFunc save_locale_;

        // 领域模块
        std::unique_ptr<domain::ContainerManager> container_mgr_;
        std::unique_ptr<domain::Environment> environment_;
        std::unique_ptr<domain::TermuxManager> termux_;
        std::unique_ptr<domain::GUIManager> gui_;
        std::unique_ptr<domain::MirrorManager> mirror_mgr_;
        std::unique_ptr<domain::BackupManager> backup_mgr_;
        std::unique_ptr<domain::DockerManager> docker_mgr_;
        std::unique_ptr<domain::VirtualizationManager> virt_mgr_;
        std::unique_ptr<domain::ConfigManager> config_mgr_;
        std::unique_ptr<domain::SoftwareCenter> software_center_;
        std::unique_ptr<domain::TerminalAppManager> terminal_app_;
        std::unique_ptr<domain::OfficeManager> office_;
        std::unique_ptr<domain::EducationManager> education_;
        std::unique_ptr<domain::InputMethodManager> input_method_;
        std::unique_ptr<domain::BrowserManager> browser_;
        std::unique_ptr<domain::BetaFeaturesManager> beta_features_;
        std::unique_ptr<domain::DeveloperTools> dev_tools_;
        std::unique_ptr<domain::DownloadTools> download_tools_;


        /** 通用容器启动辅助方法 (Proot/Chroot/Nspawn)。 */
        int launch_container(const LaunchContext &ctx, domain::ContainerMode mode,
                             const std::string &mode_label, bool needs_root);

        /** 将各领域模块注册到 MenuRegistry 并构建主菜单。 */
        void register_plugins();

        /** 首次启动初始化：检查/安装核心依赖（aria2c/sudo/curl 等），幂等（标记文件防重复）。 */
        void ensure_initialized();

        /** 构建主菜单（Termux 10 项 / Linux 7 项）。 */
        std::shared_ptr<tmoe::ui::IUIMenu> build_root_menu();

        /** 子菜单构建器。 */
        std::shared_ptr<tmoe::ui::IUIMenu> build_faq_menu();

        std::shared_ptr<tmoe::ui::IUIMenu> build_locale_menu();

        std::shared_ptr<tmoe::ui::IUIMenu> build_mirror_menu();

        void select_mirror_from_category(const std::string &category);

        /** 容器操作辅助方法（原 tui_routes_ 内联逻辑）。 */
        void action_proot_container();

        void action_chroot_container();

        void action_remove_container();

        void action_self_update();

        void action_bug_report();
    };
} // namespace tmoe::app
