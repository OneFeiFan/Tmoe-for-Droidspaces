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
    void install_firefox();
    void install_chromium();
    void install_edge();
    void install_falkon();
    void install_vivaldi();
    void install_midori();
    void install_epiphany();
    void create_no_sandbox_wrapper(const std::string& name, const std::string& bin_name);
    void ensure_firefox_ppa();
    void ensure_chromium_ppa();
    void ensure_edge_repo();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
