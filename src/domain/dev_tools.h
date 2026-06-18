#ifndef DEV_TOOLS_H
#define DEV_TOOLS_H
#pragma once
#include "core/config.h"
#include <string>
#include <vector>

namespace tmoe::domain {

class DeveloperTools {
public:
    explicit DeveloperTools(const TmoeConfig& cfg);
    void run_dev_tools_menu();

private:
    void run_build_menu();
    void run_editor_menu();
    void run_language_menu();
    void run_lang_python();
    void run_lang_nodejs();
    void run_lang_java();
    void run_lang_go();
    void run_lang_rust();
    void run_lang_ruby();
    void run_lang_php();
    void run_lang_perl();
    void run_lang_lua();
    void run_database_menu();
    void run_web_server_menu();
    void run_network_menu();
    void run_shell_enhance_menu();
    void run_monitoring_menu();

    void install_pkg(const std::string& i18n_key, const std::string& pkg);
    void install_pkgs(const std::vector<std::string>& pkgs);

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
