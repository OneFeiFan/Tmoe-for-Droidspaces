#ifndef BETA_FEATURES_H
#define BETA_FEATURES_H
#pragma once

#include "core/config.h"
#include "core/executor.h"
#include <string>
#include <vector>
#include <functional>

namespace tmoe::domain {

/** Secret Garden (秘密花园) — 对应 Bash: old/tools/app/beta_features (773行, 12个子选项)
 *
 *  菜单结构 (与 Bash 版本完全一致):
 *   1  🐬 container/vm    → 委托给 VirtualizationManager
 *   2  🌌 science&edu     → 委托给 EducationManager
 *   3  🔨 system          → 内嵌或委托
 *   4  🌼 Store&download  → 内嵌子菜单
 *   5  🎬 cut video       → 内嵌子菜单
 *   6  🎨 paint           → 内嵌子菜单
 *   7  💾 file            → 内嵌子菜单
 *   8  📝 read            → 内嵌子菜单
 *   9  🥅 network         → 占位 (待实现)
 *  10  ⌨  input method    → 委托给 InputMethodManager
 *  11  >_ Terminal        → 委托给 TerminalAppManager
 *  12  🍕 other           → 内嵌子菜单
 */
    class BetaFeaturesManager {
    public:
        explicit BetaFeaturesManager(const TmoeConfig &cfg);

        // ── 回调注入 (从 Manager 注入，用于跨模块委托) ──
        void set_virt_callback(std::function<void()> cb) { virt_cb_ = std::move(cb); }

        void set_education_callback(std::function<void()> cb) { education_cb_ = std::move(cb); }

        void set_input_method_callback(std::function<void()> cb) { input_method_cb_ = std::move(cb); }

        void set_terminal_callback(std::function<void()> cb) { terminal_cb_ = std::move(cb); }

        // ── 跨模块委托调用 (供 UI 插件使用) ──
        void virt_delegate() { if (virt_cb_) virt_cb_(); }

        void education_delegate() { if (education_cb_) education_cb_(); }

        void input_method_delegate() { if (input_method_cb_) input_method_cb_(); }

        void terminal_delegate() { if (terminal_cb_) terminal_cb_(); }

        /** 选项9: 网络管理 — 对应 Bash old/tools/system/network (321行)
         *  包含 nmtui, 设备管理, WiFi扫描, 网卡驱动, IP查看, 蓝牙等 11 个子选项 */
        void run_network_menu();

        /** 通用单包安装（供插件层内嵌子菜单使用）。*/
        void install_single(const std::string &i18n_key, const std::string &pkg);

        /** 通用多包安装（供插件层内嵌子菜单使用）。*/
        void install_multi(const std::vector<std::string> &pkgs);

    private:
        // ── 网络管理子功能 ──
        void ensure_nm_tools();

        void enable_network_device();

        void wifi_scan();

        void network_device_status();

        void install_network_driver();

        void view_ip_address();

        void install_wifi_qr_tool();

        void edit_network_config();

        void toggle_nm_autostart();

        void install_blueman_pkg();

        void install_gnome_nettool_pkg();

        const TmoeConfig &cfg_;

        // 跨模块回调
        std::function<void()> virt_cb_;
        std::function<void()> education_cb_;
        std::function<void()> input_method_cb_;
        std::function<void()> terminal_cb_;

    };

} // namespace tmoe::domain
#endif
