#ifndef OFFICE_H
#define OFFICE_H
#pragma once
#include "core/config.h"
#include "domain/apps/office_apps.h"

namespace tmoe::domain {

/** Office 办公软件管理器 — 持有 App 对象并提供 cfg 访问。
 *  插件层直接调用 mgr->wps.install() 等，无需薄委托方法。 */
class OfficeManager {
public:
    explicit OfficeManager(const TmoeConfig& cfg);

    // 公开的 App 对象 — 插件层直接调用 .install()
    LibreOfficeApp   libreoffice;
    LibreOfficeZhApp libreoffice_zh;
    WpsApp           wps;
    YozoApp          yozo;
    SimpleApp        freeoffice;
    SimpleApp        meld;
    SimpleApp        kdiff3;
    ManpagesZhApp    manpages_zh;

    // LibreOffice 中文版选择（with_zh 分派到不同 App）
    void install_libreoffice(bool with_zh);

private:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
