#ifndef BROWSER_H
#define BROWSER_H
#pragma once
#include "core/config.h"
#include "domain/apps/browser_apps.h"

namespace tmoe::domain {
    /** 浏览器管理器 — 持有 App 对象并提供 cfg 访问。
     *  插件层直接调用 mgr->chromium.install() 等，无需薄委托方法。 */
    class BrowserManager {
    public:
        explicit BrowserManager(const TmoeConfig &cfg);

        const TmoeConfig &cfg() const { return cfg_; }

        // 公开的 App 对象 — 插件层直接调用 .install() / .remove()
        ChromiumApp chromium;
        FirefoxApp firefox;
        FirefoxESRApp firefox_esr;
        EdgeApp edge;
        FalkonApp falkon;
        VivaldiApp vivaldi;
        SimpleApp epiphany;
        SimpleApp midori;

    private:
        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
#endif
