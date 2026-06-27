#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "../gui/vnc_manager.h"

namespace tmoe::domain::desktop_utils {
    void install_noto_fonts(DistroFamily family, bool is_debian) {
        if (!is_debian) return;
        PackageManager::install({"fonts-noto-cjk", "fonts-noto-color-emoji"}, family);
    }

    void install_language_packs(const TmoeConfig &cfg) {
        if (cfg.sub_distro != "ubuntu" && cfg.linux_distro != "debian") return;
        auto lang = std::string(I18n::current_lang());
        if (lang.find("zh") != 0) return;

        std::vector<std::string> pkgs = {"language-pack-zh-hans"};
        auto family = infer_family_from_config(cfg.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();

        if (Executor::has("startplasma-x11") || Executor::has("plasmashell"))
            pkgs.emplace_back("language-pack-kde-zh-hans");
        if (Executor::has("gnome-shell") || Executor::has("gnome-session"))
            pkgs.emplace_back("language-pack-gnome-zh-hans");

        PackageManager::install(pkgs, family);
    }

    void purge_libfprint_and_clean(bool is_proot, bool is_debian) {
        if (!is_proot || !is_debian) return;
        Executor::passthrough("apt purge -y ^libfprint 2>/dev/null || true");
        Executor::passthrough("apt clean 2>/dev/null || true");
        Executor::passthrough("apt autoclean 2>/dev/null || true");
    }

    void dpkg_configure_and_keyboard(bool is_debian) {
        if (!is_debian) return;
        Executor::passthrough("dpkg --configure -a 2>/dev/null || true");
    }

    void remove_udisks_gvfs_for_proot(
        std::string_view desktop_id, bool is_proot, bool is_debian) {
        if (!is_proot || !is_debian) return;
        std::string d(desktop_id);
        std::transform(d.begin(), d.end(), d.begin(), ::tolower);
        if (d.find("xfce") != std::string::npos ||
            d.find("lxde") != std::string::npos ||
            d.find("lxqt") != std::string::npos) {
            Executor::passthrough(
                "apt purge -y --allow-change-held-packages "
                "^udisks2 ^gvfs ^gvfs-backends ^gvfs-daemons 2>/dev/null || true");
        }
    }

    std::string resolve_distro_pkg_list(
        const DesktopInfo &info, DistroFamily family) {
        std::string key = PackageManager::family_key(family);
        if (!info.distro_pkgs.empty()) {
            auto it = info.distro_pkgs.find(key);
            if (it != info.distro_pkgs.end()) return it->second;
        }
        return info.pkg_group;
    }
} // namespace tmoe::domain::desktop_utils
