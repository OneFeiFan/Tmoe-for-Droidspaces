#include "domain/apps/office.h"
#include "core/logger.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

OfficeManager::OfficeManager(const TmoeConfig &cfg)
    : cfg_(cfg),
      libreoffice_(cfg),
      libreoffice_zh_(cfg),
      wps_(cfg),
      yozo_(cfg),
      freeoffice_(cfg, "FreeOffice", "freeoffice"),
      meld_(cfg, "Meld", "meld"),
      kdiff3_(cfg, "KDiff3", "kdiff3"),
      manpages_zh_(cfg) {
}

// ═══════════════════════════════════════════════════════════════
// 薄委托 — 所有安装逻辑委托给 InstallableApp 子类
// ═══════════════════════════════════════════════════════════════

void OfficeManager::install_libreoffice(bool with_zh) {
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (with_zh)
        libreoffice_zh_.install(family);
    else
        libreoffice_.install(family);
}

void OfficeManager::install_wps() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    wps_.install(family);
}

void OfficeManager::install_yozo() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    yozo_.install(family);
}

void OfficeManager::install_freeoffice() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    freeoffice_.install(family);
}

void OfficeManager::install_meld() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    meld_.install(family);
}

void OfficeManager::install_kdiff3() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    kdiff3_.install(family);
}

void OfficeManager::install_manpages_zh() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    manpages_zh_.install(family);
}

} // namespace tmoe::domain
