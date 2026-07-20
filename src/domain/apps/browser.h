#ifndef BROWSER_H
#define BROWSER_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {
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
        // 辅助
        void create_no_sandbox_wrapper(const std::string &name, const std::string &bin_name);
        void ensure_firefox_ppa();
        void ensure_chromium_ppa();
        void ensure_edge_repo();

        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
#endif
