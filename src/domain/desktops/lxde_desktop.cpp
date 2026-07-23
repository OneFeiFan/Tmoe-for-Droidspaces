#include "lxde_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "domain/system/package_manager.h"
#include "ui/dialog_helpers.h"

namespace tmoe::domain {

    LxdeDesktop::LxdeDesktop(const TmoeConfig &cfg)
            : DesktopBase(cfg, gui_config::all_desktops()[3])  // lxde is idx 3
    {}

    void LxdeDesktop::will_be_installed_message() const {
        Logger::info("LXDE: lxsession / startlxde");
        Logger::info(_("gui.lxde.package_list"));
        // Bash: apt-cache show 包信息预览
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Debian && Executor::has("apt-cache"))
            Executor::passthrough("apt-cache show lxde-core 2>/dev/null | head -20 || true");
    }

    PreInstallChoices LxdeDesktop::pre_install_choices(
            DistroFamily family, bool is_auto_mode) {

        PreInstallChoices c;
        if (is_auto_mode || family != DistroFamily::Debian) return c;

        // Ubuntu: 先选 lubuntu-desktop
        if (cfg_.sub_distro == "ubuntu") {
            if (ui::dialog::yesno(cfg_, "Lxde or Lubuntu-desktop", _("gui.desktop.lxde_or_lubuntu_desc"), "lxde",
                                  "lubuntu") == 1) {
                c.pkg_list = "lubuntu-desktop";
                return c;
            }
        }

        if (ui::dialog::yesno(cfg_, "LXDE-CORE or LXDE-LITE", _("gui.desktop.lxde_core_or_lite_desc"), "core",
                              "lite") != 0) {
            c.use_no_recommends = true;
            c.pkg_list = "pcmanfm lxterminal openbox-lxde-session lxde-icon-theme lxpanel";
        }
        return c;
    }

    void LxdeDesktop::post_install_config(const PostInstallContext &ctx) {
        // Bash: do_you_want_to_continue 终端 Y/N 确认
        if (!Logger::confirm_yes_default(_("gui.lxde.confirm_install"))) return;

        // 非 Debian 发行版时显示不支持的警告
        if (!ctx.is_debian && ctx.family == DistroFamily::Solus)
            Logger::warn(_("app.solus_not_supported"));

        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        desktop_utils::remove_udisks_gvfs_for_proot(get_id(), ctx.is_proot, ctx.is_debian);
        if (ctx.is_debian)
            desktop_utils::install_noto_fonts(ctx.family, true);
        desktop_utils::install_language_packs(cfg_);
    }

} // namespace tmoe::domain
