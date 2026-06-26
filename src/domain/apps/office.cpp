#include "domain/apps/office.h"
#include "core/i18n.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    OfficeManager::OfficeManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void OfficeManager::run_office_menu() {
        std::string menu = cfg_.tui_bin + " --title \"" + _("office.menu_title")
                           + "\" --menu \"" + _("office.menu_prompt") + "\" 0 0 0 "
                           "\"1\" \"" + _("office.libreoffice") + "\" "
                           "\"2\" \"" + _("office.libreoffice_zh") + "\" "
                           "\"3\" \"" + _("office.wps") + "\" "
                           "\"4\" \"" + _("office.yozo") + "\" "
                           "\"5\" \"" + _("office.freeoffice") + "\" "
                           "\"6\" \"" + _("office.meld") + "\" "
                           "\"7\" \"" + _("office.kdiff3") + "\" "
                           "\"8\" \"" + _("office.manpages_zh") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") install_libreoffice(false);
        else if (ch == "2") install_libreoffice(true);
        else if (ch == "3") install_wps();
        else if (ch == "4") install_yozo();
        else if (ch == "5") install_freeoffice();
        else if (ch == "6") install_meld();
        else if (ch == "7") install_kdiff3();
        else if (ch == "8") install_manpages_zh();
        Logger::press_enter();
    }

    void OfficeManager::install_libreoffice(bool with_zh) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::vector<std::string> pkgs = {"libreoffice", "libreoffice-gtk3"};
        if (with_zh) {
            pkgs.push_back("libreoffice-l10n-zh-cn");
        }
        Logger::step(_(with_zh ? "office.libreoffice_zh" : "office.libreoffice"));
        PackageManager::install(pkgs, family);
        apply_libreoffice_proot_patch();
    }

    void OfficeManager::install_wps() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("office.wps"));
        PackageManager::install("wps-office", family);
        ensure_wps_fonts();
        refresh_font_cache();
    }

    void OfficeManager::install_yozo() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::info(_("office.yozo_license"));
        Logger::step(_("office.yozo"));
        PackageManager::install("yozo-office", family);
    }

    void OfficeManager::install_freeoffice() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("office.freeoffice"));
        PackageManager::install("freeoffice", family);
    }

    void OfficeManager::install_meld() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("office.meld"));
        PackageManager::install("meld", family);
    }

    void OfficeManager::install_kdiff3() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("office.kdiff3"));
        PackageManager::install("kdiff3", family);
    }

    void OfficeManager::install_manpages_zh() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("office.manpages_zh"));
        PackageManager::install({"manpages-zh", "man-db", "debian-reference-zh-cn"}, family);
    }

    void OfficeManager::apply_libreoffice_proot_patch() {
        if (!cfg_.is_termux && !cfg_.is_root) return;

        Logger::step(_("office.proot_patch"));
        Executor::shell(
            "if [ -f /usr/lib/libreoffice/program/oosplash ] && "
            "[ ! -L /usr/lib/libreoffice/program/oosplash ]; then "
            "mv /usr/lib/libreoffice/program/oosplash "
            "/usr/lib/libreoffice/program/oosplash.bak 2>/dev/null; "
            "ln -sf /usr/lib/libreoffice/program/soffice "
            "/usr/lib/libreoffice/program/oosplash 2>/dev/null; fi"
        );
    }

    void OfficeManager::ensure_wps_fonts() {
        Logger::step(_("office.wps_fonts"));
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install("ttf-wps-fonts", family);
        // msttcorefonts 仅在 Debian 有
        if (PackageManager::is_command_available("apt"))
            PackageManager::install("ttf-mscorefonts-installer", DistroFamily::Debian);
    }

    void OfficeManager::refresh_font_cache() {
        Logger::step(_("office.font_cache"));
        CommandBuilder("fc-cache").add_flag("-fv").add_raw("2>/dev/null || true").execute();
    }
} // namespace tmoe::domain
