#ifndef BETA_FEATURES_H
#define BETA_FEATURES_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class BetaFeaturesManager {
public:
    explicit BetaFeaturesManager(const TmoeConfig& cfg);
    void run_beta_menu();

private:
    void run_store_menu();
    void run_deepin_menu();
    void run_drawing_menu();
    void run_rlang_menu();
    void run_file_menu();
    void run_multimedia_menu();

    void install_single(const std::string& i18n_key, const std::string& pkg);
    void install_multi(const std::vector<std::string>& keys,
                       const std::vector<std::string>& pkgs);

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
