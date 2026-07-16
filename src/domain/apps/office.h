#ifndef OFFICE_H
#define OFFICE_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class OfficeManager {
public:
    explicit OfficeManager(const TmoeConfig& cfg);
    void run_office_menu();
    void install_libreoffice(bool with_zh);
    void install_wps();
    void install_yozo();
    void install_freeoffice();
    void install_meld();
    void install_kdiff3();
    void install_manpages_zh();

private:
    void apply_libreoffice_proot_patch();
    void ensure_wps_fonts();
    void refresh_font_cache();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
