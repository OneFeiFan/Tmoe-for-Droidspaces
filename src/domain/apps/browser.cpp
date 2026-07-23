#include "domain/apps/browser.h"

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
} // namespace tmoe::domain
