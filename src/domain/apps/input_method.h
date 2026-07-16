#ifndef INPUT_METHOD_H
#define INPUT_METHOD_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class InputMethodManager {
public:
    explicit InputMethodManager(const TmoeConfig& cfg);
    void run_input_method_menu();

    void run_fcitx4_menu();
    void run_fcitx5_menu();
    void run_ibus_menu();
    void install_sogou();
    void show_input_faq();

    // ── 插件子菜单所需的细粒度操作 ──────────────────────
    void write_input_env_vars();
    void setup_fcitx_autostart();
    void install_fcitx4_engine(const std::string& pkg_name);
    void install_fcitx4_tools();
    void install_fcitx5_packages(const std::vector<std::string>& pkgs);
    void install_ibus_engine(const std::string& pkg_name);

private:

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
