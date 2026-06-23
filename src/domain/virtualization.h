#pragma once
#include "core/config.h"
#include "core/executor.h"
#include "core/logger.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {

/** 虚拟化主管理器，完全复刻 Bash 版本:
 *   tools/virtualization/virt-menu (不含 qemu/VirtualBox/genymotion)
 *   tools/virtualization/iso.sh
 *   tools/virtualization/wine32.sh
 *   Docker 管理由独立的 DockerManager 处理
 */
class VirtualizationManager {
public:
    explicit VirtualizationManager(const TmoeConfig &cfg);

    // ── TUI 入口 ──
    /** 虚拟化主菜单 (对应 Bash virt-menu install_container_and_virtual_machine) */
    void run_virt_menu();

    /** 设置 Docker 子页回调（从主 Manager 注入）。 */
    void set_docker_callback(std::function<void()> cb) { docker_cb_ = std::move(cb); }

    // ── Wine ──
    bool install_wine(std::string_view branch = "devel");
    bool install_winetricks();
    bool install_dxvk();
    bool install_playonlinux();
    void run_wine_menu();

private:
    const TmoeConfig &cfg_;
    std::function<void()> docker_cb_;
    std::string download_path_;

    // ── 下载路径 ──
    std::string get_download_path() const;
    void ensure_download_path() const;

    // ═══════════════════════════════════════════════════════
    //  qcow2 / ISO 分支 (对应 Bash choose_qcow2_or_iso)
    // ═══════════════════════════════════════════════════════

    /** 顶层分支：qcow2 模板 还是 ISO 下载？ */
    void run_qcow2_or_iso_menu();

    // ── qcow2 模板 (对应 Bash tmoe_qemu_templates_repo) ──
    void run_qcow2_templates_menu();
    void download_qcow2_template(const std::string& name,
                                 const std::string& dl_filename,
                                 const std::string& qcow2_filename,
                                 const std::string& dl_url,
                                 const std::string& size_hint);
    bool check_and_download_qcow2(const std::string& dl_filename,
                                  const std::string& dl_url);
    void uncompress_qcow2_and_setup(const std::string& dl_filename,
                                    const std::string& qemu_name,
                                    const std::string& qcow2_filename);

    // ── ISO 下载 (对应 Bash download_virtual_machine_iso_file) ──
    void run_iso_menu();

    // 各 OS 的 ISO 下载子菜单
    void download_alpine_iso();
    void download_android_x86_iso();
    void download_debian_iso();
    void download_ubuntu_iso();
    void download_windows_iso();
    void download_lmde_iso();

    // ── 通用下载模型 (对应 Bash 的多个 download_*_model 函数) ──
    bool tmoe_iso_download_model(const std::string& iso_filename,
                                 const std::string& iso_url,
                                 const std::string& hint = "");
    bool download_with_curl(const std::string& url,
                            const std::string& output_path);

    // ── Wine 版本信息 ──
    static std::vector<std::pair<std::string, std::string>> wine_branches();

    // ── 发行版检测 ──
    std::string os_release() const;
    bool is_debian() const;
    bool is_ubuntu() const;
    bool is_arch() const;

    // ── Whiptail 辅助 ──
    bool whiptail_yesno(const std::string& title,
                        const std::string& yes_btn,
                        const std::string& no_btn,
                        const std::string& prompt,
                        int height = 0, int width = 50);
};

} // namespace tmoe::domain
