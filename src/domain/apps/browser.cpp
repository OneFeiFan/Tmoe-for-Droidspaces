#include "domain/apps/browser.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

BrowserManager::BrowserManager(const TmoeConfig &cfg)
    : cfg_(cfg),
      chromium_(cfg),
      firefox_(cfg),
      firefox_esr_(cfg),
      edge_(cfg),
      falkon_(cfg),
      vivaldi_(cfg) {
}

// ═══════════════════════════════════════════════════════════════
// 薄委托 — 所有安装/卸载逻辑委托给 InstallableApp 子类
// ═══════════════════════════════════════════════════════════════

void BrowserManager::install_chromium() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    chromium_.install(family);
}

void BrowserManager::remove_chromium() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    chromium_.remove(family);
}

void BrowserManager::install_firefox() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    firefox_.install(family);
}

void BrowserManager::remove_firefox() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    firefox_.remove(family);
}

void BrowserManager::install_firefox_esr() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    if (!firefox_esr_.install(family)) {
        Logger::warn(_("browser.firefox_esr_failed_fallback"));
        firefox_.install(family);
    }
}

void BrowserManager::remove_firefox_esr() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    firefox_esr_.remove(family);
}

void BrowserManager::install_edge() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    edge_.install(family);
}

void BrowserManager::remove_edge() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    edge_.remove(family);
}

void BrowserManager::install_falkon() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    falkon_.install(family);
}

void BrowserManager::remove_falkon() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    falkon_.remove(family);
}

void BrowserManager::install_vivaldi() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    vivaldi_.install(family);
}

// ═══════════════════════════════════════════════════════════════
// 简单浏览器 — 保持直接调用（过于简单，不必子类化）
// ═══════════════════════════════════════════════════════════════

void BrowserManager::install_epiphany() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    Logger::step("Epiphany");
    PackageManager::install("epiphany-browser", family);
}

void BrowserManager::install_midori() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    Logger::step("Midori");
    PackageManager::install("midori", family);
}

} // namespace tmoe::domain
