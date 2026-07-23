#include "deepin_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"
#include <cstdlib>

#include "domain/gui/desktop_manager.h"
#include "ui/dialog_helpers.h"

namespace tmoe::domain {

    void DeepinDesktop::will_be_installed_message() const {
        Logger::info("Deepin: deepin-session / deepin-launcher");

        // Bash: dde_warning — ASCII 兼容性表格
        Logger::info(_("gui.dde.compat_table"));

        // Bash: 安装前包列表
        Logger::info(_("gui.dde.package_list"));

        // Bash: proxy_warning — proot/chroot 环境警告
        bool is_proot = cfg_.is_termux || cfg_.linux_distro == "Android";
        if (is_proot) {
            Logger::warn(_("gui.dde.proot_warning"));
        } else {
            auto family = infer_family_from_config(cfg_.linux_distro);
            if (family == DistroFamily::Unknown)
                family = PackageManager::detect_distro_family();
            if (cfg_.linux_distro == "fedora" || family == DistroFamily::RedHat) {
                Logger::info(_("gui.dde.chroot_fedora_msg"));
            } else {
                Logger::warn(_("gui.dde.chroot_other_msg"));
            }
        }

        // Bash: tips_of_tiger_vnc_server — Tiger VNC 推荐
        Logger::info(_("gui.dde.tiger_vnc_tip"));

        // Bash: do_you_want_to_continue — 终端 Y/N 确认
        if (!Logger::confirm_yes_default(_("gui.dde.confirm_install"))) {
            std::exit(0);
        }
    }

    PreInstallChoices DeepinDesktop::pre_install_choices(DistroFamily f, bool a) {
        PreInstallChoices c;
        if (a) return c;
        if (f == DistroFamily::Arch) {
            if (cfg_.arch != "amd64" && cfg_.arch != "i386")
                Logger::warn("Deepin on non-x86 Arch may have limited functionality");
            return c;
        }
        if (f == DistroFamily::RedHat) {
            c.pkg_list = "deepin-desktop";
            return c;
        }
        if (f != DistroFamily::Debian) return c;
        if (cfg_.sub_distro == "deepin") {
            c.pkg_list = "dde";
            return c;
        }
        c.pkg_list = ui::dialog::yesno(cfg_, "DDE/DDE-extras", "dde/extras?", "dde", "dde-extras") == 0
                     ? "ubuntudde-dde deepin-terminal" : "ubuntudde-dde ubuntudde-dde-extras";
        return c;
    }

    void DeepinDesktop::post_install_config(const PostInstallContext &ctx) {
        // Bash: fix_dde_dpkg_error — 修复 mincores-dkms 和 warm-sched 的 dpkg postinst 脚本
        if (ctx.is_debian) {
            Executor::shell(
                    "for i in mincores-dkms warm-sched; do "
                    "  if [ -e /var/lib/dpkg/info/$i.postinst ]; then "
                    "    sudo sed -i '1a return 0' /var/lib/dpkg/info/$i.postinst; "
                    "  fi; "
                    "done");
        }
        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        if (ctx.is_debian) {
            if (cfg_.sub_distro != "deepin")
                Executor::passthrough(
                        "sudo apt update 2>/dev/null; sudo apt install -y software-properties-common gnupg 2>/dev/null; yes | sudo add-apt-repository ppa:ubuntudde-dev/stable 2>/dev/null || true");
            desktop_utils::install_noto_fonts(ctx.family, true);
        }
        desktop_utils::install_language_packs(cfg_);
    }

    void DeepinDesktop::post_install_extras(const PostInstallContext &ctx) {
        // Deepin 专用主题和壁纸（与 DDE 共用 deepin 生态）
        auto family = ctx.family;
        if (family == DistroFamily::Debian) {
            PackageManager::install({"deepin-wallpapers", "deepin-icon-theme", "deepin-gtk-theme"}, family);
        }
        if (family == DistroFamily::Arch) {
            PackageManager::install({"deepin-wallpapers", "deepin-icon-theme", "deepin-gtk-theme"}, family);
        }
    }

} // namespace tmoe::domain
