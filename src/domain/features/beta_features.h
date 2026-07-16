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
    explicit BetaFeaturesManager(const TmoeConfig& cfg);

    /** 秘密花园主菜单入口。 */
    void run_beta_menu();

    // ── 回调注入 (从 Manager 注入，用于跨模块委托) ──
    void set_virt_callback(std::function<void()> cb)  { virt_cb_ = std::move(cb); }
    void set_education_callback(std::function<void()> cb)  { education_cb_ = std::move(cb); }
    void set_input_method_callback(std::function<void()> cb)  { input_method_cb_ = std::move(cb); }
    void set_terminal_callback(std::function<void()> cb)  { terminal_cb_ = std::move(cb); }

    // ── 跨模块委托调用 (供 UI 插件使用) ──
    void virt_delegate() { if (virt_cb_) virt_cb_(); }
    void education_delegate() { if (education_cb_) education_cb_(); }
    void input_method_delegate() { if (input_method_cb_) input_method_cb_(); }
    void terminal_delegate() { if (terminal_cb_) terminal_cb_(); }

    // ── 第1层子菜单 ──

    /** 选项3: 系统管理 (sudo用户组, rc.local, UEFI启动项, 资源监视器等) */
    void run_system_menu();

    /** 选项4: 商店与下载 (aptitude, deepin商店, gnome-software, flatpak, snap, bauh, qbittorrent) */
    void run_store_menu();

    /** 选项5: 视频剪辑 (openshot, mkvtoolnix, kdenlive, flowblade, shotcut, olive, blender) */
    void run_video_menu();

    /** 选项6: 绘图 (krita, inkscape, kolourpaint, R语言, latexdraw, librecad, freecad, opencad, kicad, openscad, gnuplot) */
    void run_paint_menu();

    /** 选项7: 文件管理 (thunar/nautilus/dolphin, catfish, gparted, baobab, cfdisk, partitionmanager, mc, ranger, gnome-disks) */
    void run_file_menu();

    /** 选项8: 阅读器 (calibre, fbreader, typora, xournal, evince, okular, kchmviewer, pdfchain) */
    void run_reader_menu();

    /** 选项9: 网络管理 (nmtui, 设备管理, WiFi扫描, 网卡驱动, IP查看, etc.) — 占位 */
    void run_network_menu();

    /** 选项12: 其他 (OBS-Studio, seahorse, kodi, scrcpy, flameshot, telegram) */
    void run_other_menu();

private:
    const TmoeConfig& cfg_;

    // 跨模块回调
    std::function<void()> virt_cb_;
    std::function<void()> education_cb_;
    std::function<void()> input_method_cb_;
    std::function<void()> terminal_cb_;

    // ── 第2层/第3层子菜单 ──

    /** Deepin 软件子菜单 (16项) */
    void run_deepin_menu();

    /** R语言子菜单 */
    void run_r_lang_menu();

    /** scrcpy 子菜单 */
    void run_scrcpy_menu();

    // ── 辅助 ──

    /** 通用单包安装 */
    void install_single(const std::string& i18n_key, const std::string& pkg);

    /** 通用多包安装 */
    void install_multi(const std::vector<std::string>& pkgs);
};

} // namespace tmoe::domain
#endif
