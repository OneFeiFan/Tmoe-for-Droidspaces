#include "domain/apps/terminal_app.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {
    TerminalAppManager::TerminalAppManager(const TmoeConfig &cfg)
        : cfg_(cfg), family_(DistroFamily::Unknown) {
        family_ = infer_family_from_config(cfg_.linux_distro);
        if (family_ == DistroFamily::Unknown)
            family_ = PackageManager::detect_distro_family();
    }

    void TerminalAppManager::install_terminal(const std::string &pkg,
                                              const std::string &label_key,
                                              const std::string &hint_key) {
        Logger::step(_(label_key));
        PackageManager::install(pkg, family_);

        if (!hint_key.empty()) {
            Logger::info(_(hint_key));
        }
        if (pkg == "cool-retro-term" && cfg_.linux_distro == "debian") {
            Logger::info(_("term.hint_cool_retro_term_failed"));
        }

        Logger::press_enter();
    }
} // namespace tmoe::domain
