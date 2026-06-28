#include "kde_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include <filesystem>

namespace tmoe::domain {
    KdeDesktop::KdeDesktop(const TmoeConfig &cfg)
        : DesktopBase(cfg)
          , info_(gui_config::all_desktops()[5]) // kde is idx 5
    {
    }

    std::string KdeDesktop::get_id() const { return info_.id; }
    const DesktopInfo &KdeDesktop::get_info() const { return info_; }

    void KdeDesktop::will_be_installed_message() const {
        Logger::info("KDE Plasma: startplasma-x11 / startkde");
    }

    // ── 阶段2: 装包前版本选择 ──
    PreInstallChoices KdeDesktop::pre_install_choices(
        DistroFamily family, bool is_auto_mode) {
        PreInstallChoices c;
        if (is_auto_mode) return c;

        if (family == DistroFamily::Debian) {
            bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
            if (is_ubuntu) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"KDE-plasma or Kubuntu-desktop\""
                                               " --yes-button \"KDE\" --no-button \"kubuntu\""
                                               " --yesno '前者为普通KDE,后者为kubuntu' 0 0");
                if (r.exit_code == 1) {
                    c.pkg_list = "kubuntu-desktop";
                    return c;
                }
            }
            // choose_kde_plasma_or_standard
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"kde-plasma or kde-standard\""
                                           " --yes-button \"plasma\" --no-button \"standard\""
                                           " --yesno '前者为精简安装,后者为标准安装' 0 0");
            if (r.exit_code == 0) {
                c.pkg_list = "kde-plasma-desktop";
            } else {
                auto r2 = Executor::passthrough(cfg_.tui_bin +
                                                " --title \"kde-standard or kde-full\""
                                                " --yes-button \"standard\" --no-button \"full\""
                                                " --yesno '前者包含KDE标准套件,后者为KDE全家桶' 0 0");
                c.pkg_list = (r2.exit_code == 0) ? "kde-standard" : "kde-full";
            }
        } else if (family == DistroFamily::Arch) {
            auto r = Executor::passthrough(cfg_.tui_bin +
                                           " --title \"kde-plasma or plasma-meta\""
                                           " --yes-button \"plasma\" --no-button \"plasma+apps\""
                                           " --yesno '前者为plasma基础桌面,后者包含kde全家桶' 0 0");
            c.pkg_list = (r.exit_code == 0)
                             ? "plasma-desktop dolphin konsole discover"
                             : "plasma-meta kde-applications-meta plasma-wayland-session sddm sddm-kcm";
        }
        return c;
    }

    // ── 阶段3: 系统配置 ──
    void KdeDesktop::post_install_config(const PostInstallContext &ctx) {
        kde_warning();
        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        if (ctx.is_debian) desktop_utils::install_noto_fonts(ctx.family, true);

        // proot: 去掉 systemd 相关组件
        if (ctx.is_proot) {
            PackageManager::remove("plasma-powerdevil", ctx.family);
            PackageManager::remove("plasma-discover", ctx.family);
            PackageManager::remove("plasma-discover-backend-flatpak", ctx.family);
            PackageManager::remove("plasma-discover-backend-snap", ctx.family);
        }

        // wayland / x11 选择
        if (ctx.is_debian && !false /*auto_install_mode_ not available here*/) {
            choose_wayland_or_x11(ctx);
        }

        if (ctx.is_debian) Executor::passthrough("sudo apt clean 2>/dev/null || true");
        desktop_utils::install_language_packs(cfg_);
    }

    // ═══════════════════════════════════════════
    // private
    // ═══════════════════════════════════════════

    void KdeDesktop::kde_warning() const {
        Logger::info(_("gui.kde.warning"));
    }

    void KdeDesktop::choose_wayland_or_x11(const PostInstallContext & /*ctx*/) {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --title \"x11 or wayland\" --yes-button \"x11\" --no-button \"wayland\""
                                       " --yesno '默认推荐x11, wayland尚在实验阶段' 0 0");
        if (r.exit_code == 1) {
            plasma_wayland_env();
            Logger::info(_("gui.plasma_wayland.selected"));
        }
    }

    void KdeDesktop::plasma_wayland_env() {
        std::string home = SystemHelper::user_home();
        std::string profile_file = home + "/.profile";

        auto existing = SystemHelper::read_file(fs::path(profile_file));
        std::string append_content;
        if (existing.find("XDG_SESSION_TYPE=wayland") == std::string::npos)
            append_content += "export XDG_SESSION_TYPE=wayland\n";
        if (existing.find("GDK_BACKEND=wayland") == std::string::npos)
            append_content += "export GDK_BACKEND=wayland\n";
        if (existing.find("QT_QPA_PLATFORM=wayland") == std::string::npos)
            append_content += "export QT_QPA_PLATFORM=wayland\n";
        if (existing.find("MOZ_ENABLE_WAYLAND=1") == std::string::npos)
            append_content += "export MOZ_ENABLE_WAYLAND=1\n";

        if (!append_content.empty()) {
            SystemHelper::append_file(fs::path(profile_file),
                                      "\n# tmoe-linux Plasma Wayland\n" + append_content);
            Logger::ok(_("gui.plasma_wayland.configured"));
        }
    }
} // namespace tmoe::domain
