#ifndef DEV_TOOLS_H
#define DEV_TOOLS_H
#pragma once
#include "core/config.h"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {
    /** 开发工具菜单 — 1:1 复刻原版 Bash dev-menu + vscode 脚本。
     *
     *  原版调用链路:
     *    software_center → dev_menu() → source dev-menu
     *      └─ development_programming_tools() (11项IDE菜单)
     *           ├─ source vscode → which_vscode_edition() (4项VS Code子菜单)
     *           ├─ dev_menu_01() → install_ide_01 / delete_ide_pkg / remove_ide_01
     *           ├─ dev_menu_02() → install_ide_02 / delete_ide_pkg_02 / remove_ide_02
     *           ├─ install_emacs() / install_code_blocks() → beta_features_quick_install
     *           └─ install_sublime_text_stable()
     */
    class DeveloperTools {
    public:
        explicit DeveloperTools(const TmoeConfig &cfg);

        // ── 插件可访问的菜单入口 ─────────────────────────────────
        // (由 IUIMenu 框架的 LambdaAction 调用，替代旧 whiptail 菜单)

        /** Android Studio → 设置变量后进入 dev_menu_02 */
        void prep_android_studio();

        /** IDEA 旗舰版 vs 社区版选择 */
        void choose_idea_edition();

        /** GNU Emacs 快速安装 */
        void install_emacs();

        /** Code::Blocks 快速安装 */
        void install_code_blocks();

        /** Sublime Text 安装 (添加官方源 + 安装) */
        void install_sublime_text();

        /** PyCharm Community → 设置状态后进入 dev_menu_01 */
        void prep_pycharm();

        /** WebStorm → 设置状态后进入 dev_menu_01 */
        void prep_webstorm();

        /** CLion → 设置状态后进入 dev_menu_01 */
        void prep_clion();

        /** GoLand → 设置状态后进入 dev_menu_01 */
        void prep_goland();

        /** GitHub Desktop → 设置状态后进入 dev_menu_01 */
        void prep_github_desktop();

        // ── 插件子菜单所需的细粒度操作 ──────────────────────
        // (原为私有方法，现开放给 IUIMenu 框架的 LambdaAction 调用)

        /** VS Code 官方版安装 (Microsoft CDN) */
        void install_vscode_official();

        /** VS Code Server (code-server Web版) */
        void run_vscode_server_menu();

        /** VS Codium 安装 */
        void install_vscodium();

        /** 修复 tightvnc 下 vscode 无法启动 */
        void fix_tightvnc_vscode();

        /** dev_menu_01: JetBrains 等 tar.zst 安装 (从 archlinuxcn) */
        void install_ide_01();

        /** dev_menu_02: Android Studio tar.gz 安装 */
        void install_ide_02();

        /** 删除下载的安装包 (dev_menu_01) */
        void delete_ide_pkg();

        /** 删除下载的安装包 (dev_menu_02) */
        void delete_ide_pkg_02();

        /** 卸载 IDE (dev_menu_01) */
        void remove_ide_01();

        /** 卸载 IDE (dev_menu_02) */
        void remove_ide_02();

    private:
        // ═══════════════════════════════════════════════════════
        // VS Code Server 内部实现
        // ═══════════════════════════════════════════════════════

        void configure_vscode_server();

        void vscode_server_upgrade();

        void vscode_server_restart();

        void vscode_server_password();

        void vscode_server_remove();

        void install_github_desktop();

        // ═══════════════════════════════════════════════════════
        // 辅助方法
        // ═══════════════════════════════════════════════════════

        /** 确保下载目录存在 */
        void check_download_path();

        /** 从 archlinuxcn 抓取最新 .pkg.tar.zst 版本号 */
        std::string fetch_latest_archlinuxcn_version();

        /** 根据 grep_name_ 返回 JetBrains 产品代码 (空=非 JetBrains) */
        std::string jetbrains_product_code() const;

        /** 通过 JetBrains API 获取最新版直链 + 版本号 */
        bool fetch_jetbrains_link(std::string &out_url, std::string &out_version);

        /** 下载并安装 JetBrains tar.gz (API 直链) */
        bool download_and_extract_jetbrains(const std::string &url, const std::string &filename);

        /** 下载 Android Studio tar.gz (url与文件名由调用方抓取后传入，避免重复请求) */
        bool download_android_studio(const std::string &url, const std::string &filename);

        /** 解压并安装 Android Studio */
        bool extract_android_studio();

        /** 创建 Android Studio .desktop 文件 */
        void create_android_studio_desktop();

        /** 为已安装的 JetBrains IDE 创建 .desktop 桌面图标 */
        void create_jetbrains_desktop();

        /** 安装 Java 运行时 (JetBrains IDE 依赖) */
        void install_java_if_needed();

        /** 确认用户是否继续 (调用 Logger::confirm 或 whiptail yesno) */
        bool confirm(const std::string &msg_key, const std::string &fallback);

        /** 显示 "想要对 xxx 做什么" 的版本信息表 */
        void show_ide_version_table(const std::string &latest_ver, const std::string &local_ver);

        /** 检查本地已安装版本 */
        std::string check_local_opt_version();

        /** 打印手动安装提示 */
        void tip_manual_install(const std::string &url);

        /** 下载单个文件到指定路径 */
        bool download_file(const std::string &url, const std::string &dest_path);

        /** 使用 aria2c 下载并实时显示进度条，下载完成后验证文件存在且非空。
         *  @param url  下载地址
         *  @param dest 完整目标路径 (含文件名)
         *  @param continue_on_exists 若文件已存在且非空，跳过下载
         *  @return 下载成功或文件已存在
         */
        bool aria2_download(const std::string &url,
                            const std::string &dest,
                            bool continue_on_exists = true);

        // ═══════════════════════════════════════════════════════
        // 状态变量 (对应 Bash 中的全局变量)
        // ═══════════════════════════════════════════════════════
        const TmoeConfig &cfg_;

        std::string grep_name_; // GREP_NAME: 应用标识符
        std::string lnk_file_; // LNK_FILE: .desktop 文件名
        std::string bin_file_; // BIN_FILE: 二进制路径
        std::string icon_name_; // ICON_NAME: 图标名
        std::string app_opt_dir_; // APP_OPT_DIR: /opt/xxx 安装目录
        std::string icon_file_; // ICON_FILE: 完整图标路径

        bool community_edition_ = false; // COMMUNITY_EDITION
        int dev_menu_type_ = 0; // DEV_MENU: 0=主菜单, 1=jetbrains, 2=android_studio

        fs::path download_path_; // DOWNLOAD_PATH
        fs::path apps_lnk_dir_; // APPS_LNK_DIR

        /** 重置所有状态变量 */
        void reset_state();
    };
} // namespace tmoe::domain
#endif
