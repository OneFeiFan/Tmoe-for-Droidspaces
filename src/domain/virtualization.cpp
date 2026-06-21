#include "virtualization.h"
#include "core/i18n.h"

namespace tmoe::domain {
    VirtualizationManager::VirtualizationManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  TUI 主菜单 — 对应 Bash: tools/virtualization/virt-menu
    // ═══════════════════════════════════════════════════════════════

    void VirtualizationManager::run_virt_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("virt.title") + "\""
                               " --menu \"" + _("virt.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("virt.download_iso") + "\" "
                               "\"2\" \"" + _("virt.docker") + "\" "
                               "\"3\" \"" + _("virt.wine") + "\" "
                               "\"4\" \"" + _("virt.virt_manager") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1") {
                run_iso_menu();
            } else if (choice == "2") {
                if (docker_cb_) docker_cb_();
            } else if (choice == "3") {
                run_wine_menu();
            } else if (choice == "4") {
                install_virt_manager();
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════
    //  共享工具
    // ═══════════════════════════════════════════════════════════════

    fs::path VirtualizationManager::vm_store_dir() const {
        return cfg_.work_dir / "vm-store";
    }

    void VirtualizationManager::ensure_vm_dir() const {
        fs::create_directories(vm_store_dir());
    }

    // ═══════════════════════════════════════════════════════════════
    //  ISO 下载 — 对应 Bash: tools/virtualization/iso.sh
    // ═══════════════════════════════════════════════════════════════

    std::vector<VirtualizationManager::IsoInfo>
    VirtualizationManager::available_isos() {
        return {
            {
                "alpine",
                "https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/x86_64/alpine-standard-x86_64.iso",
                "Alpine Linux — 最小体积 (~200MB)"
            },
            {
                "debian", "https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-12.0.0-amd64-netinst.iso",
                "Debian 12 — 网络安装 (~600MB)"
            },
            {
                "ubuntu", "https://releases.ubuntu.com/24.04/ubuntu-24.04-desktop-amd64.iso",
                "Ubuntu 24.04 LTS — 桌面版 (~5GB)"
            },
            {
                "android-x86",
                "https://sourceforge.net/projects/android-x86/files/Release%209.0/android-x86_64-9.0-r2.iso",
                "Android x86_64 9.0"
            },
            {
                "lmde", "https://mirrors.edge.kernel.org/linuxmint/debian/lmde-6-cinnamon-64bit.iso",
                "LMDE 6 — Linux Mint Debian Edition"
            },
        };
    }

    bool VirtualizationManager::download_iso(std::string_view os_name) {
        auto isos = available_isos();
        const IsoInfo *target = nullptr;

        for (const auto &iso: isos) {
            if (iso.name == os_name) {
                target = &iso;
                break;
            }
        }

        if (!target) {
            Logger::error(_f("virt.unsupported_iso", std::string(os_name)));
            return false;
        }

        ensure_vm_dir();
        fs::path iso_path = vm_store_dir() / (std::string(os_name) + ".iso");

        Logger::step(_f("virt.downloading_iso", std::string(os_name)));
        Logger::info(_f("virt.iso_source", target->url));
        Logger::info(_f("virt.iso_save_path", iso_path.string()));
        Logger::info(_("virt.iso_download_hint"));

        // 使用 wget 或 curl 下载
        std::string dl_cmd = "wget -O \"" + iso_path.string() +
                             "\" \"" + target->url + "\" 2>&1";
        // 如果 wget 不可用，回退到 curl
        if (!Executor::has("wget")) {
            dl_cmd = "curl -L -o \"" + iso_path.string() +
                     "\" \"" + target->url + "\"";
        }

        auto result = Executor::passthrough(dl_cmd);
        if (result.ok() && fs::exists(iso_path)) {
            auto size_mb = fs::file_size(iso_path) / (1024 * 1024);
            Logger::ok(_f("virt.iso_download_ok", iso_path.string(), std::to_string(size_mb)));
            return true;
        }

        Logger::error(_("virt.iso_download_failed"));
        return false;
    }

    void VirtualizationManager::run_iso_menu() {
        while (true) {
            auto isos = available_isos();

            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("virt.iso_title") + "\""
                               " --menu \"" + _("virt.iso_prompt") + "\" 0 0 0 ";

            for (size_t i = 0; i < isos.size(); ++i) {
                menu += "\"" + std::to_string(i + 1) + "\" \""
                        + isos[i].name + " — " + isos[i].description + "\" ";
            }

            menu += "\"0\" \"" + _("menu.tui.back") + "\"";

            std::string choice = Executor::tui_select(menu);
            if (choice == "0" || choice.empty()) break;

            int idx = std::stoi(choice) - 1;
            if (idx >= 0 && idx < static_cast<int>(isos.size())) {
                download_iso(isos[idx].name);
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Wine — 对应 Bash: tools/virtualization/wine32.sh
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
            // 启用 32 位架构
            Executor::passthrough("dpkg --add-architecture i386 2>/dev/null");
            Executor::passthrough(cfg_.update_command);

            // 安装 WineHQ
            auto result = Executor::passthrough(cfg_.install_command +
                                                " wine wine32 wine64 2>/dev/null");

            if (!result.ok()) {
                // 从 WineHQ 官方源安装
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

        // 通用包管理方式
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
        return Executor::passthrough("winetricks dxvk 2>/dev/null").ok();
    }

    bool VirtualizationManager::install_playonlinux() {
        Logger::step(_("virt.installing_pol"));
        return Executor::passthrough(cfg_.install_command + " playonlinux").ok();
    }

    void VirtualizationManager::run_wine_menu() {
        while (true) {
            auto branches = wine_branches();

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

    bool VirtualizationManager::install_virt_manager() {
        Logger::step(_("virt.installing_virt_manager"));
        auto result = Executor::passthrough(cfg_.install_command +
                                            " virt-manager libvirt-daemon-system");
        if (result.ok()) {
            Logger::ok(_("virt.virt_manager_installed"));
            Executor::passthrough("systemctl enable libvirtd 2>/dev/null");
            Executor::passthrough("systemctl start libvirtd 2>/dev/null");
            return true;
        }
        return false;
    }

    std::string VirtualizationManager::os_release() const {
        auto result = Executor::shell("cat /etc/os-release 2>/dev/null | grep '^ID=' | cut -d= -f2");
        std::string rel = result.ok() ? result.stdout_data : "";
        rel.erase(std::remove(rel.begin(), rel.end(), '\n'), rel.end());
        rel.erase(std::remove(rel.begin(), rel.end(), '\"'), rel.end());
        return rel;
    }

    bool VirtualizationManager::is_debian() const {
        return os_release() == "debian";
    }

    bool VirtualizationManager::is_ubuntu() const {
        return os_release() == "ubuntu";
    }

    bool VirtualizationManager::is_arch() const {
        return os_release() == "arch";
    }
} // namespace tmoe::domain
