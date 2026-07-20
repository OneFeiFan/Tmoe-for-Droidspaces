#include "kde_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include <filesystem>
#include "ui/dialog_helpers.h"

namespace tmoe::domain {
    KdeDesktop::KdeDesktop(const TmoeConfig &cfg)
        : DesktopBase(cfg)
          , info_(gui_config::all_desktops()[5]) // kde is idx 5
    {
    }

    std::string KdeDesktop::get_id() const { return info_.id; }
    const DesktopInfo &KdeDesktop::get_info() const { return info_; }

    void KdeDesktop::will_be_installed_message() const {
        // Bash: 终端 printf 详细包列表
        Logger::info("KDE Plasma: startplasma-x11 / startkde");
        Logger::info(_("gui.kde.package_list"));
    }

    // ── 阶段2: 装包前版本选择 ──
    PreInstallChoices KdeDesktop::pre_install_choices(
        DistroFamily family, bool is_auto_mode) {
        PreInstallChoices c;
        if (is_auto_mode) return c;

        if (family == DistroFamily::Debian) {
            bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
            if (is_ubuntu) {
                if (ui::dialog::yesno(cfg_, "KDE-plasma or Kubuntu-desktop", "前者为普通KDE,后者为kubuntu", "KDE", "kubuntu") == 1) {
                    c.pkg_list = "kubuntu-desktop";
                    return c;
                }
            }
            // choose_kde_plasma_or_standard
            if (ui::dialog::yesno(cfg_, "kde-plasma or kde-standard", "前者为精简安装,后者为标准安装", "plasma", "standard") == 0) {
                c.pkg_list = "kde-plasma-desktop";
            } else {
                c.pkg_list = ui::dialog::yesno(cfg_, "kde-standard or kde-full", "前者包含KDE标准套件,后者为KDE全家桶", "standard", "full") == 0
                                 ? "kde-standard" : "kde-full";
            }
        } else if (family == DistroFamily::Arch) {
            c.pkg_list = ui::dialog::yesno(cfg_, "kde-plasma or plasma-meta", "前者为plasma基础桌面,后者包含kde全家桶", "plasma", "plasma+apps") == 0
                             ? "plasma-desktop dolphin konsole discover"
                             : "plasma-meta kde-applications-meta plasma-wayland-session sddm sddm-kcm";
        }
        return c;
    }

    // ── 阶段3: 系统配置 ──
    void KdeDesktop::post_install_config(const PostInstallContext &ctx) {
        if (!kde_warning()) return;
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

    bool KdeDesktop::kde_warning() const {
        // Bash: catimg 预览图 + ASCII 兼容性表格 + proot 警告 + tiger 推荐 + do_you_want_to_continue
        Logger::info("------------------------");
        Logger::info(_("gui.kde.warning"));
        Logger::info("");

        // Bash: proot 环境特殊警告
        if (cfg_.is_termux || cfg_.linux_distro == "Android") {
            Logger::warn("PROOT " + std::string(_("gui.kde.proot_warn")));
            Logger::info("  " + std::string(_("gui.kde.proot_dolphin_hint")));
            Logger::info("  " + std::string(_("gui.kde.proot_startup_warn")));
        }

        // Bash: tips_of_tiger_vnc_server — 推荐 tiger VNC
        Logger::info(_("gui.kde.tiger_recommend"));
        Logger::info("");

        // Bash: do_you_want_to_continue — 终端 read [Y/n]
        Logger::info(_("gui.kde.warning.continue"));
        if (!Logger::confirm_yes_default(_("gui.kde.warning.continue"))) return false;
        return true;
    }

    void KdeDesktop::choose_wayland_or_x11(const PostInstallContext & /*ctx*/) {
        if (ui::dialog::yesno(cfg_, "x11 or wayland", "默认推荐x11, wayland尚在实验阶段", "x11", "wayland") == 1) {
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
    void KdeDesktop::post_install_extras(const PostInstallContext& /*ctx*/) {
        // 安装 Breeze 主题全套（Bash 原版在 KDE 安装后自动安装）
        desktop_utils::install_breeze_theme_ext(cfg_);
    }

} // namespace tmoe::domain
