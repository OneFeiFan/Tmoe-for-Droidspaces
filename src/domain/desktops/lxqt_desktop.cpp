#include "lxqt_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    LxqtDesktop::LxqtDesktop(const TmoeConfig &cfg)
        : DesktopBase(cfg)
          , info_(gui_config::all_desktops()[2]) // lxqt is idx 2
    {
    }

    std::string LxqtDesktop::get_id() const { return info_.id; }
    const DesktopInfo &LxqtDesktop::get_info() const { return info_; }

    void LxqtDesktop::will_be_installed_message() const {
        Logger::info("LXQt: lxqt-session / startlxqt");
    }

    PreInstallChoices LxqtDesktop::pre_install_choices(
        DistroFamily family, bool is_auto_mode) {
        PreInstallChoices c;
        if (is_auto_mode || family != DistroFamily::Debian) return c;

        bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
        if (is_ubuntu) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"Lxqt or Lubuntu-desktop\""
                                           " --yes-button \"lxqt\" --no-button \"lubuntu\""
                                           " --yesno '前者为普通lxqt,后者为lubuntu' 0 0");
            if (r.exit_code == 1) {
                c.pkg_list = "lubuntu-desktop";
                return c;
            }
        }

        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"LXQT-CORE or LXQT-LITE\""
                                       " --yes-button \"core\" --no-button \"lite\""
                                       " --yesno '前者为普通lxqt,后者为精简版lxqt' 0 0");
        if (r.exit_code != 0) {
            c.use_no_recommends = true;
            c.pkg_list = "pcmanfm-qt pcmanfm-qt-l10n qterminal qterminal-l10n "
                    "openbox lxqt-theme-debian lxqt-panel lxqt-config "
                    "lxqt-session-l10n lxqt-session";
        }
        return c;
    }

    void LxqtDesktop::post_install_config(const PostInstallContext &ctx) {
        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        if (ctx.is_debian)
            desktop_utils::install_noto_fonts(ctx.family, true);
        desktop_utils::install_language_packs(cfg_);
    }
} // namespace tmoe::domain
