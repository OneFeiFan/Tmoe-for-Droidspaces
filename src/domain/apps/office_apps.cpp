#include "domain/apps/office_apps.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

// ═══════════════════════════════════════════════════════════════
// LibreOfficeApp
// ═══════════════════════════════════════════════════════════════

void LibreOfficeApp::apply_proot_patch() {
    if (!cfg_.is_termux && !cfg_.is_root) return;

    Logger::step(_("office.proot_patch"));
    Executor::shell(
        "if [ -f /usr/lib/libreoffice/program/oosplash ] && "
        "[ ! -L /usr/lib/libreoffice/program/oosplash ]; then "
        "sudo mv /usr/lib/libreoffice/program/oosplash "
        "/usr/lib/libreoffice/program/oosplash.bak 2>/dev/null; "
        "sudo ln -sf /usr/lib/libreoffice/program/soffice "
        "/usr/lib/libreoffice/program/oosplash 2>/dev/null; fi"
    );
}

bool LibreOfficeApp::install(DistroFamily family) {
    Logger::step(_("office.libreoffice"));
    std::vector<std::string> pkgs = {"libreoffice", "libreoffice-gtk3"};
    if (!PackageManager::install(pkgs, family)) return false;
    apply_proot_patch();
    return true;
}

// ═══════════════════════════════════════════════════════════════
// LibreOfficeZhApp
// ═══════════════════════════════════════════════════════════════

void LibreOfficeZhApp::apply_proot_patch() {
    if (!cfg_.is_termux && !cfg_.is_root) return;

    Logger::step(_("office.proot_patch"));
    Executor::shell(
        "if [ -f /usr/lib/libreoffice/program/oosplash ] && "
        "[ ! -L /usr/lib/libreoffice/program/oosplash ]; then "
        "sudo mv /usr/lib/libreoffice/program/oosplash "
        "/usr/lib/libreoffice/program/oosplash.bak 2>/dev/null; "
        "sudo ln -sf /usr/lib/libreoffice/program/soffice "
        "/usr/lib/libreoffice/program/oosplash 2>/dev/null; fi"
    );
}

bool LibreOfficeZhApp::install(DistroFamily family) {
    Logger::step(_("office.libreoffice_zh"));
    std::vector<std::string> pkgs = {"libreoffice", "libreoffice-gtk3", "libreoffice-l10n-zh-cn"};
    if (!PackageManager::install(pkgs, family)) return false;
    apply_proot_patch();
    return true;
}

// ═══════════════════════════════════════════════════════════════
// WpsApp
// ═══════════════════════════════════════════════════════════════

void WpsApp::ensure_fonts(DistroFamily family) {
    Logger::step(_("office.wps_fonts"));
    PackageManager::install("ttf-wps-fonts", family);
    if (PackageManager::is_command_available("apt"))
        PackageManager::install("ttf-mscorefonts-installer", DistroFamily::Debian);
}

void WpsApp::refresh_font_cache() {
    Logger::step(_("office.font_cache"));
    CommandBuilder("sudo").add_arg("fc-cache").add_flag("-fv")
        .add_raw("2>/dev/null || true").execute();
}

bool WpsApp::install(DistroFamily family) {
    Logger::step(_("office.wps"));
    if (!PackageManager::install("wps-office", family)) return false;
    ensure_fonts(family);
    refresh_font_cache();
    return true;
}

// ═══════════════════════════════════════════════════════════════
// YozoApp — 安装前显示 license 提示
// ═══════════════════════════════════════════════════════════════

bool YozoApp::pre_install(DistroFamily /*family*/) {
    Logger::info(_("office.yozo_license"));
    return true;
}

} // namespace tmoe::domain
