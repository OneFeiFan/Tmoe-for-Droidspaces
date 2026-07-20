#ifndef BROWSER_H
#define BROWSER_H
#pragma once
#include "core/config.h"
#include "domain/apps/browser_apps.h"
#include <string>

namespace tmoe::domain {
    /** 浏览器管理器 — 对外部（UI 插件层）提供统一入口，内部委托给 InstallableApp 子类。 */
    class BrowserManager {
    public:
        explicit BrowserManager(const TmoeConfig &cfg);

        /** 全局配置访问器（供 UI 层使用 dialog::yesno 等工具）。 */
        const TmoeConfig& cfg() const { return cfg_; }

        // ── 安装 ──
        void install_firefox();
        void install_firefox_esr();
        void install_chromium();
        void install_edge();
        void install_falkon();
        void install_vivaldi();
        void install_epiphany();
        void install_midori();

        // ── 卸载 ──
        void remove_chromium();
        void remove_edge();
        void remove_falkon();
        void remove_firefox();
        void remove_firefox_esr();

    private:
        const TmoeConfig &cfg_;
        ChromiumApp    chromium_;
        FirefoxApp     firefox_;
        FirefoxESRApp  firefox_esr_;
        EdgeApp        edge_;
        FalkonApp      falkon_;
        VivaldiApp     vivaldi_;
    };
} // namespace tmoe::domain
#endif
