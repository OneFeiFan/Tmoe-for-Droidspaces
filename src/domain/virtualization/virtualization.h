#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <functional>
#include <string>
#include <vector>

namespace tmoe::domain {

/** 虚拟化主管理器 — Docker + Wine 两大入口。
 *  qemu / VirtualBox / ISO下载 / genymotion 已移除。
 */
class VirtualizationManager {
public:
    explicit VirtualizationManager(const TmoeConfig &cfg);

    /** 虚拟化主菜单 (Docker / Wine)。 */
    void run_virt_menu();

    void set_docker_callback(std::function<void()> cb) { docker_cb_ = std::move(cb); }

    /** 触发 Docker 回调（供 UI 插件使用，回调由 Manager 构造函数注入）。 */
    void invoke_docker() { if (docker_cb_) docker_cb_(); }

    // ── Wine ──
    bool install_wine(std::string_view branch = "devel");
    bool install_winetricks();
    bool install_dxvk();
    bool install_playonlinux();
    void run_wine_menu();

private:
    const TmoeConfig &cfg_;
    std::function<void()> docker_cb_;

    static std::vector<std::pair<std::string, std::string>> wine_branches();

    std::string os_release() const;
    bool is_debian() const;
    bool is_ubuntu() const;
    bool is_arch() const;
};

} // namespace tmoe::domain
