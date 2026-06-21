#ifndef BROWSER_H
#define BROWSER_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class BrowserManager {
public:
    explicit BrowserManager(const TmoeConfig& cfg);
    void run_browser_menu();

private:
    // 旧 Bash 菜单结构
    void firefox_or_chromium();
    void firefox_or_firefoxesr();
    void chromium_browser_menu();
    void microsoft_edge_menu();
    void falkon_browser_menu();

    // 安装
    void install_firefox();
    void install_firefox_esr();
    void install_chromium();
    void install_edge();
    void install_falkon();
    void install_vivaldi();
    void install_midori();
    void install_epiphany();

    // 卸载
    void remove_chromium();
    void remove_edge();
    void remove_falkon();
    void remove_firefox();
    void remove_firefox_esr();

    // 辅助
    void create_no_sandbox_wrapper(const std::string& name, const std::string& bin_name);
    void ensure_firefox_ppa();
    void ensure_chromium_ppa();
    void ensure_edge_repo();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
