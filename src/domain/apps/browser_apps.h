/** 浏览器 InstallableApp 子类。
 *
 *  简单浏览器（Falkon、Vivaldi）使用基类默认管线，只需声明包名 + 沙箱包装标志。
 *  复杂浏览器（Chromium、Firefox、Firefox ESR、Edge）覆盖 install()/remove()
 *  以处理发行版特有的 PPA/GPG/仓库配置和复杂清理逻辑。
 */
#pragma once
#include "domain/apps/installable_app.h"

namespace tmoe::domain {

// ═══════════════════════════════════════════════════════════════
// 简单浏览器 — 使用默认 install/remove 管线
// ═══════════════════════════════════════════════════════════════

class FalkonApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Falkon"; }
    std::string bin_name() const override { return "falkon"; }
    DistroPkgNames packages() const override { return {.common = "falkon"}; }
    bool needs_sandbox_wrapper() const override { return true; }
};

class VivaldiApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Vivaldi"; }
    std::string bin_name() const override { return "vivaldi-stable"; }
    DistroPkgNames packages() const override {
        return {.arch = "vivaldi", .common = "vivaldi-stable"};
    }
    bool needs_sandbox_wrapper() const override { return true; }
};

// ═══════════════════════════════════════════════════════════════
// 复杂浏览器 — 覆盖 install()/remove() 处理发行版特有逻辑
// ═══════════════════════════════════════════════════════════════

class ChromiumApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Chromium"; }
    std::string bin_name() const override { return "chromium"; }
    DistroPkgNames packages() const override { return {.common = "chromium"}; }
    bool needs_sandbox_wrapper() const override { return true; }

    using InstallableApp::install;
    using InstallableApp::remove;
    bool install(DistroFamily family) override;
    bool remove(DistroFamily family) override;

private:
    void ensure_ppa();
};

class FirefoxApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Firefox"; }
    std::string bin_name() const override { return "firefox"; }
    DistroPkgNames packages() const override { return {.common = "firefox"}; }
    bool needs_sandbox_wrapper() const override { return true; }

    using InstallableApp::install;
    using InstallableApp::remove;
    bool install(DistroFamily family) override;
    bool remove(DistroFamily family) override;
};

class FirefoxESRApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Firefox ESR"; }
    std::string bin_name() const override { return "firefox-esr"; }
    DistroPkgNames packages() const override { return {.common = "firefox-esr"}; }
    bool needs_sandbox_wrapper() const override { return true; }

    using InstallableApp::install;
    using InstallableApp::remove;
    bool install(DistroFamily family) override;
    bool remove(DistroFamily family) override;
};

class EdgeApp : public InstallableApp {
public:
    using InstallableApp::InstallableApp;
    std::string name() const override { return "Microsoft Edge"; }
    std::string bin_name() const override { return "microsoft-edge-dev"; }
    DistroPkgNames packages() const override { return {.common = "microsoft-edge-dev"}; }
    bool needs_sandbox_wrapper() const override { return true; }

    using InstallableApp::install;
    using InstallableApp::remove;
    bool install(DistroFamily family) override;
    bool remove(DistroFamily family) override;

private:
    void ensure_repo();
};

} // namespace tmoe::domain
