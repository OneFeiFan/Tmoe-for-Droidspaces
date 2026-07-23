#include "domain/virtualization/virtualization.h"
#include "core/i18n.h"
#include "domain/system/package_manager.h"
#include "core/system_helper.h"
#include <algorithm>

namespace tmoe::domain {
    VirtualizationManager::VirtualizationManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  qcow2 / ISO 分支 (对应 Bash choose_qcow2_or_iso, iso.sh:3-9)
    // ═══════════════════════════════════════════════════════════════

    std::vector<std::pair<std::string, std::string> >
    VirtualizationManager::wine_branches() {
        return {
                {"devel",   _("virt.wine_branch_devel")},
                {"staging", _("virt.wine_branch_staging")},
                {"stable",  _("virt.wine_stable")},
        };
    }

    bool VirtualizationManager::install_wine(std::string_view branch) {
        Logger::step(_f("virt.installing_wine", std::string(branch)));

        if (is_debian() || is_ubuntu()) {
            Executor::passthrough("sudo dpkg --add-architecture i386 2>/dev/null");
            PackageManager::update(DistroFamily::Debian);

            auto result = PackageManager::install({"wine", "wine32", "wine64"}, DistroFamily::Debian);

            if (!result) {
                Executor::passthrough("sudo mkdir -p /etc/apt/keyrings");
                Executor::passthrough("wget -qO- https://dl.winehq.org/wine-builds/winehq.key | "
                                      "gpg --dearmor -o /etc/apt/keyrings/winehq-archive-keyring.gpg");
                PackageManager::update(DistroFamily::Debian);

                std::string whq = "winehq-" + std::string(branch);
                std::string wb = "wine-" + std::string(branch);
                result = PackageManager::install({whq, wb, wb + "-amd64", wb + "-i386"},
                                                 DistroFamily::Debian);
            }

            if (result) {
                Logger::ok(_f("virt.wine_installed", std::string(branch)));
                Logger::info(_("virt.wine_winecfg_hint"));
                return true;
            }
        }

        if (is_arch()) {
            auto result = PackageManager::install("wine", DistroFamily::Arch);
            if (result) {
                Logger::ok(_("virt.wine_installed_simple"));
                return true;
            }
        }

        auto result = PackageManager::install("wine");
        if (result) {
            Logger::ok(_("virt.wine_installed_simple"));
            return true;
        }

        Logger::error(_("virt.wine_install_failed"));
        return false;
    }

    bool VirtualizationManager::install_winetricks() {
        Logger::step(_("virt.installing_winetricks"));
        auto result = PackageManager::install("winetricks");
        if (result) {
            Logger::ok(_("virt.winetricks_installed"));
        } else {
            Logger::warn(_("virt.winetricks_fallback"));
            Executor::passthrough("sudo wget -O /usr/local/bin/winetricks "
                                  "https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks 2>/dev/null");
            Executor::shell("sudo chmod +x /usr/local/bin/winetricks");
        }
        return true;
    }

    bool VirtualizationManager::install_dxvk() {
        Logger::step(_("virt.installing_dxvk"));
        Logger::info(_("virt.dxvk_hint"));

        // Wine 不能以 root 运行。用 $SUDO_USER 或从实际家目录反查真实用户
        std::string real_user;
        const char *sudo_user = std::getenv("SUDO_USER");
        if (sudo_user && sudo_user[0]) real_user = sudo_user;
        else {
            std::string home = SystemHelper::user_home();
            auto r = Executor::shell(
                    std::string("grep \"") + home + "\" /etc/passwd | awk -F':' '{print $1}' | head -n1");
            if (r.ok()) {
                real_user = r.stdout_data;
                real_user.erase(std::remove(real_user.begin(), real_user.end(), '\n'), real_user.end());
            }
        }

        std::string cmd = real_user.empty() || real_user == "root"
                          ? "winetricks dxvk 2>/dev/null"
                          : "su " + real_user + " -c \"winetricks dxvk\" 2>/dev/null";
        return Executor::passthrough(cmd).ok();
    }

    bool VirtualizationManager::install_playonlinux() {
        Logger::step(_("virt.installing_pol"));
        return PackageManager::install("playonlinux");
    }

    // ═══════════════════════════════════════════════════════════════
    //  辅助
    // ═══════════════════════════════════════════════════════════════

    std::string VirtualizationManager::os_release() const {
        auto result = Executor::shell("cat /etc/os-release 2>/dev/null | grep '^ID=' | cut -d= -f2");
        std::string rel = result.ok() ? result.stdout_data : "";
        rel.erase(std::remove(rel.begin(), rel.end(), '\n'), rel.end());
        rel.erase(std::remove(rel.begin(), rel.end(), '\"'), rel.end());
        return rel;
    }

    bool VirtualizationManager::is_debian() const { return os_release() == "debian"; }

    bool VirtualizationManager::is_ubuntu() const { return os_release() == "ubuntu"; }

    bool VirtualizationManager::is_arch() const { return os_release() == "arch"; }
} // namespace tmoe::domain
