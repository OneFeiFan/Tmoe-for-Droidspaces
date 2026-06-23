#include "virtualization.h"
#include "core/i18n.h"
#include <algorithm>

namespace tmoe::domain {

    VirtualizationManager::VirtualizationManager(const TmoeConfig &cfg) : cfg_(cfg) {}

    // ═══════════════════════════════════════════════════════════════
    //  主菜单 — Docker / Wine
    // ═══════════════════════════════════════════════════════════════

    void VirtualizationManager::run_virt_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("virt.title") + "\""
                               " --menu \"" + _("virt.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("virt.docker") + "\" "
                               "\"2\" \"" + _("virt.wine") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                if (docker_cb_) docker_cb_();
            } else if (choice == "2") {
                run_wine_menu();
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  qcow2 / ISO 分支 (对应 Bash choose_qcow2_or_iso, iso.sh:3-9)
    // ═══════════════════════════════════════════════════════════════

    std::vector<std::pair<std::string, std::string> >
    VirtualizationManager::wine_branches() {
        return {
            {"devel", "Wine 开发版 — 最新特性"},
            {"staging", "Wine Staging — 实验补丁"},
            {"stable", "Wine 稳定版"},
        };
    }

    bool VirtualizationManager::install_wine(std::string_view branch) {
        Logger::step(_f("virt.installing_wine", std::string(branch)));

        if (is_debian() || is_ubuntu()) {
            Executor::passthrough("dpkg --add-architecture i386 2>/dev/null");
            Executor::passthrough(cfg_.update_command);

            auto result = Executor::passthrough(cfg_.install_command +
                                                " wine wine32 wine64 2>/dev/null");

            if (!result.ok()) {
                Executor::passthrough("mkdir -p /etc/apt/keyrings");
                Executor::passthrough("wget -qO- https://dl.winehq.org/wine-builds/winehq.key | "
                    "gpg --dearmor -o /etc/apt/keyrings/winehq-archive-keyring.gpg");
                Executor::passthrough(cfg_.update_command);

                result = Executor::passthrough(cfg_.install_command + " --install-recommends "
                                               "winehq-" + std::string(branch) + " wine-" +
                                               std::string(branch) + " wine-" +
                                               std::string(branch) + "-amd64 wine-" +
                                               std::string(branch) + "-i386");
            }

            if (result.ok()) {
                Logger::ok(_f("virt.wine_installed", std::string(branch)));
                Logger::info(_("virt.wine_winecfg_hint"));
                return true;
            }
        }

        if (is_arch()) {
            auto result = Executor::passthrough("pacman -S --noconfirm wine");
            if (result.ok()) {
                Logger::ok(_("virt.wine_installed_simple"));
                return true;
            }
        }

        auto result = Executor::passthrough(cfg_.install_command + " wine 2>/dev/null");
        if (result.ok()) {
            Logger::ok("Wine 已安装");
            return true;
        }

        Logger::error(_("virt.wine_install_failed"));
        return false;
    }

    bool VirtualizationManager::install_winetricks() {
        Logger::step(_("virt.installing_winetricks"));
        auto result = Executor::passthrough(cfg_.install_command + " winetricks");
        if (result.ok()) {
            Logger::ok(_("virt.winetricks_installed"));
        } else {
            Logger::warn(_("virt.winetricks_fallback"));
            Executor::passthrough("wget -O /usr/local/bin/winetricks "
                "https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks 2>/dev/null");
            Executor::shell("chmod +x /usr/local/bin/winetricks");
        }
        return true;
    }

    bool VirtualizationManager::install_dxvk() {
        Logger::step(_("virt.installing_dxvk"));
        Logger::info(_("virt.dxvk_hint"));

        // Wine 不能以 root 运行。用 $SUDO_USER 或从 $HOME 反查真实用户
        std::string real_user;
        const char* sudo_user = std::getenv("SUDO_USER");
        if (sudo_user && sudo_user[0]) real_user = sudo_user;
        else {
            const char* home = std::getenv("HOME");
            if (home) {
                auto r = Executor::shell(
                    std::string("grep \"") + home + "\" /etc/passwd | awk -F':' '{print $1}' | head -n1");
                if (r.ok()) { real_user = r.stdout_data;
                    real_user.erase(std::remove(real_user.begin(), real_user.end(), '\n'), real_user.end()); }
            }
        }

        std::string cmd = real_user.empty() || real_user == "root"
            ? "winetricks dxvk 2>/dev/null"
            : "su " + real_user + " -c \"winetricks dxvk\" 2>/dev/null";
        return Executor::passthrough(cmd).ok();
    }

    bool VirtualizationManager::install_playonlinux() {
        Logger::step(_("virt.installing_pol"));
        return Executor::passthrough(cfg_.install_command + " playonlinux").ok();
    }

    void VirtualizationManager::run_wine_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("virt.wine_title") + "\""
                               " --menu \"" + _("virt.wine_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("virt.wine_devel") + "\" "
                               "\"2\" \"" + _("virt.wine_staging") + "\" "
                               "\"3\" \"" + _("virt.wine_stable") + "\" "
                               "\"4\" \"" + _("virt.wine_winetricks") + "\" "
                               "\"5\" \"" + _("virt.wine_dxvk") + "\" "
                               "\"6\" \"" + _("virt.wine_playonlinux") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") install_wine("devel");
            else if (choice == "2") install_wine("staging");
            else if (choice == "3") install_wine("stable");
            else if (choice == "4") install_winetricks();
            else if (choice == "5") install_dxvk();
            else if (choice == "6") install_playonlinux();

            Logger::press_enter();
        }
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
