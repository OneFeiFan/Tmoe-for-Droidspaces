#ifndef OFFICE_H
#define OFFICE_H
#pragma once
#include "core/config.h"
#include "domain/apps/office_apps.h"
#include <string>

namespace tmoe::domain {

/** Office 办公软件管理器 — 对外部（UI 插件层）提供统一入口，内部委托给 InstallableApp 子类。 */
class OfficeManager {
public:
    explicit OfficeManager(const TmoeConfig& cfg);

    void install_libreoffice(bool with_zh);
    void install_wps();
    void install_yozo();
    void install_freeoffice();
    void install_meld();
    void install_kdiff3();
    void install_manpages_zh();

private:
    const TmoeConfig& cfg_;
    LibreOfficeApp    libreoffice_;
    LibreOfficeZhApp  libreoffice_zh_;
    WpsApp            wps_;
    YozoApp           yozo_;
    SimpleApp         freeoffice_;
    SimpleApp         meld_;
    SimpleApp         kdiff3_;
    ManpagesZhApp     manpages_zh_;
};

} // namespace tmoe::domain
#endif
