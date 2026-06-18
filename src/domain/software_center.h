#ifndef SOFTWARE_CENTER_H
#define SOFTWARE_CENTER_H
#pragma once
#include "core/config.h"
#include <string>
#include <vector>

namespace tmoe::domain {

class SoftwareCenter {
public:
    explicit SoftwareCenter(const TmoeConfig& cfg);
    void run_software_center_menu();

private:
    void run_sns_menu();
    void run_games_menu();
    void run_media_menu();
    void run_docs_menu();
    void run_fileshare_menu();
    void run_pkg_gui_menu();
    void run_cleanup_menu();

    void install_or_remove(const std::string& i18n_key,
                           const std::string& pkg, bool install);
    void download_app(const std::string& name, const std::string& url);

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
