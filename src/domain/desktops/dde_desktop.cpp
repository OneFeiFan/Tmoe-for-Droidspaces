#include "dde_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"

#include "domain/gui/desktop_manager.h"
#include "ui/dialog_helpers.h"

namespace tmoe::domain {

void DdeDesktop::will_be_installed_message() const {
    Logger::info("DDE: startdde / dde-launcher");

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
}

bool DdeDesktop::dde_warning() const {
    // Bash: do_you_want_to_continue — 终端 Y/N 确认
    if (!Logger::confirm_yes_default(_("gui.dde.confirm_install"))) {
        Logger::warn(_("gui.dde.install_cancelled"));
        return false;
    }
    return true;
}

PreInstallChoices DdeDesktop::pre_install_choices(DistroFamily f, bool a) {
    PreInstallChoices c; if (a) return c;
    if (f == DistroFamily::Arch) {
        // bash: arm64 架构装不全 deepin-extra
        if (cfg_.arch != "amd64" && cfg_.arch != "i386")
            Logger::warn("DDE on non-x86 Arch may have limited functionality");
        return c; // registry handles arch pkg list
    }
    if (f == DistroFamily::RedHat) {
        c.pkg_list = "deepin-desktop";
        return c;
    }
    if (f != DistroFamily::Debian) return c;
    if (cfg_.sub_distro == "deepin") { c.pkg_list = "dde"; return c; }
    c.pkg_list = ui::dialog::yesno(cfg_, "DDE/DDE-extras", "dde/extras?", "dde", "dde-extras") == 0
                     ? "ubuntudde-dde deepin-terminal" : "ubuntudde-dde ubuntudde-dde-extras";
    return c;
}

void DdeDesktop::post_install_config(const PostInstallContext& ctx) {
    if (!dde_warning()) return;

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
        // 非 deepin 发行版需要先添加 UbuntuDDE PPA
        if (cfg_.sub_distro != "deepin") {
            Executor::passthrough(
                "sudo apt update 2>/dev/null; "
                "sudo apt install -y software-properties-common gnupg 2>/dev/null; "
                "yes | sudo add-apt-repository ppa:ubuntudde-dev/stable 2>/dev/null || true");
        }
        desktop_utils::install_noto_fonts(ctx.family, true);
    }
    desktop_utils::install_language_packs(cfg_);
}

void DdeDesktop::post_install_extras(const PostInstallContext& ctx) {
    // DDE 专用壁纸和主题
    auto family = ctx.family;
    if (family == DistroFamily::Debian) {
        PackageManager::install({"deepin-wallpapers", "dde-introduction", "deepin-icon-theme"}, family);
        // UbuntuDDE 额外壁纸
        if (ctx.is_ubuntu) {
            PackageManager::install("ubuntu-wallpapers", family);
        }
    }
    if (family == DistroFamily::Arch) {
        PackageManager::install({"deepin-wallpapers", "deepin-icon-theme"}, family);
    }
}

} // namespace tmoe::domain
