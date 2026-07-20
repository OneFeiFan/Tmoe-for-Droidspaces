#include "domain/apps/browser.h"
#include "core/i18n.h"
#include "core/logger.h"

namespace tmoe::domain {
    BrowserManager::BrowserManager(const TmoeConfig &cfg)
        : cfg_(cfg),
          chromium(cfg),
          firefox(cfg),
          firefox_esr(cfg),
          edge(cfg),
          falkon(cfg),
          vivaldi(cfg),
          epiphany(cfg, "Epiphany", "epiphany-browser"),
          midori(cfg, "Midori", "midori") {
    }

    // ESR 安装失败时回退到普通 Firefox
    void BrowserManager::install_firefox_esr() {
        if (!firefox_esr.install()) {
            Logger::warn(_("browser.firefox_esr_failed_fallback"));
            firefox.install();
        }
    }
} // namespace tmoe::domain
