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

private:
    void run_fcitx4_menu();
    void run_fcitx5_menu();
    void run_ibus_menu();
    void install_sogou();
    void write_input_env_vars();
    void setup_fcitx_autostart();
    void show_input_faq();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
