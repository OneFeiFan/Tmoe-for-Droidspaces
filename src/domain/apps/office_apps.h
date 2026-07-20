/** Office 应用 InstallableApp 子类。 */
#pragma once
#include "domain/apps/installable_app.h"

namespace tmoe::domain {

// ═══════════════════════════════════════════════════════════════
// LibreOffice（标准版）
// ═══════════════════════════════════════════════════════════════

class LibreOfficeApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "LibreOffice"; }
    DistroPkgNames packages() const override { return {.common = "libreoffice"}; }

    using InstallableApp::install;
    bool install(DistroFamily family) override;

private:
    void apply_proot_patch();
};

// ═══════════════════════════════════════════════════════════════
// LibreOffice（中文版）
// ═══════════════════════════════════════════════════════════════

class LibreOfficeZhApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "LibreOffice (zh_CN)"; }
    DistroPkgNames packages() const override { return {.common = "libreoffice"}; }

    using InstallableApp::install;
    bool install(DistroFamily family) override;

private:
    void apply_proot_patch();
};

// ═══════════════════════════════════════════════════════════════
// WPS Office
// ═══════════════════════════════════════════════════════════════

class WpsApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "WPS Office"; }
    DistroPkgNames packages() const override { return {.common = "wps-office"}; }

    using InstallableApp::install;
    bool install(DistroFamily family) override;

private:
    void ensure_fonts(DistroFamily family);
    static void refresh_font_cache();
};

// ═══════════════════════════════════════════════════════════════
// Yozo Office — 简单（仅 pre_install 显示 license）
// ═══════════════════════════════════════════════════════════════

class YozoApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Yozo Office"; }
    DistroPkgNames packages() const override { return {.common = "yozo-office"}; }
    bool pre_install(DistroFamily) override;
};

// ═══════════════════════════════════════════════════════════════
// Manpages 中文 — 多包包名
// ═══════════════════════════════════════════════════════════════

class ManpagesZhApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Manpages (zh_CN)"; }
    DistroPkgNames packages() const override {
        return {.common = "manpages-zh man-db debian-reference-zh-cn"};
    }
};

} // namespace tmoe::domain
