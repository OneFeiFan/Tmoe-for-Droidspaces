#include "deepin_desktop.h"
#include "desktop_utils.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    PreInstallChoices DeepinDesktop::pre_install_choices(DistroFamily f, bool a) {
        PreInstallChoices c;
        if (a) return c;
        if (f == DistroFamily::Arch) {
            if (cfg_.arch != "amd64" && cfg_.arch != "i386")
                Logger::warn("Deepin on non-x86 Arch may have limited functionality");
            return c;
        }
        if (f != DistroFamily::Debian) return c;
        if (cfg_.sub_distro == "deepin") {
            c.pkg_list = "dde";
            return c;
        }
        auto r = Executor::passthrough(
            cfg_.tui_bin +
            " --title \"DDE/DDE-extras\" --yes-button \"dde\" --no-button \"dde-extras\" --yesno 'dde/extras?' 0 0");
        c.pkg_list = (r.exit_code == 0) ? "ubuntudde-dde deepin-terminal" : "ubuntudde-dde ubuntudde-dde-extras";
        return c;
    }

    void DeepinDesktop::post_install_config(const PostInstallContext &ctx) {
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
void DeepinDesktop::post_install_extras(const PostInstallContext& ctx) {
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
