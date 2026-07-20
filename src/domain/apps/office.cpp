#include "domain/apps/office.h"

namespace tmoe::domain {
    OfficeManager::OfficeManager(const TmoeConfig &cfg)
        : cfg_(cfg),
          libreoffice(cfg),
          libreoffice_zh(cfg),
          wps(cfg),
          yozo(cfg),
          freeoffice(cfg, "FreeOffice", "freeoffice"),
          meld(cfg, "Meld", "meld"),
          kdiff3(cfg, "KDiff3", "kdiff3"),
          manpages_zh(cfg) {
    }

    // LibreOffice 中文版选择
    void OfficeManager::install_libreoffice(bool with_zh) {
        if (with_zh)
            libreoffice_zh.install();
        else
            libreoffice.install();
    }
} // namespace tmoe::domain
