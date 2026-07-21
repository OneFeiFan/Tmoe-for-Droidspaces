#include "domain/features/beta_features.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/str_utils.h"
#include "ui/dialog_helpers.h"
#include <algorithm>
#include <sstream>

namespace tmoe::domain {
    BetaFeaturesManager::BetaFeaturesManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项9: 网络管理 — 对应 Bash old/tools/system/network (321行)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::ensure_nm_tools() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (!Executor::has("nmtui")) {
            std::string nm_pkg;
            if (cfg_.linux_distro == "debian") nm_pkg = "network-manager";
            else if (cfg_.linux_distro == "redhat") nm_pkg = "NetworkManager-tui";
            else nm_pkg = "networkmanager";
            PackageManager::install(nm_pkg, family);
        }
        if (!Executor::has("ip")) {
            PackageManager::install("iproute2", family);
        }
        // 确保 managed=true
        if (Executor::shell("grep -q 'managed=false' /etc/NetworkManager/NetworkManager.conf 2>/dev/null").ok()) {
            Executor::shell("sudo sed -i 's@managed=false@managed=true@' /etc/NetworkManager/NetworkManager.conf");
        }
        // 启动 NetworkManager
        if (!Executor::shell("pgrep NetworkManager >/dev/null 2>&1").ok()) {
            if (cfg_.linux_distro == "alpine") {
                Executor::passthrough("sudo service networkmanager start 2>/dev/null || true");
            } else {
                Executor::passthrough("sudo systemctl start NetworkManager 2>/dev/null || "
                    "sudo service NetworkManager start 2>/dev/null || "
                    "sudo service networkmanager start 2>/dev/null || true");
            }
        }
    }

    void BetaFeaturesManager::run_network_menu() {
        ensure_nm_tools();

        Logger::info(_("beta.net_will_cover"));
        Logger::info(_("beta.net_will_cover2"));
        Logger::info(_("beta.net_will_cover3"));
        Logger::info("");

        auto family = infer_family_from_config(cfg_.linux_distro);
        bool running = true;
        while (running) {
            std::string choice = Executor::tui_select(
                cfg_.tui_bin + " --title \"NETWORK\" --menu \""
                "Configure network / 网络配置\" 17 50 11 "
                "\"1\" \"nmtui: NetworkManager TUI\" "
                "\"2\" \"enable device / 启用设备\" "
                "\"3\" \"WiFi scan / 扫描\" "
                "\"4\" \"device status / 设备状态\" "
                "\"5\" \"driver / 网卡驱动\" "
                "\"6\" \"View IP / 查看IP\" "
                "\"7\" \"wifi-qr / WiFi二维码\" "
                "\"8\" \"edit config / 手动编辑\" "
                "\"9\" \"systemctl enable / 开机自启\" "
                "\"10\" \"blueman / 蓝牙管理器\" "
                "\"11\" \"gnome-nettool / 网络工具\" "
                "\"0\" \"Return / 返回\" "
                "3>&1 1>&2 2>&3");
            if (choice.empty()) choice = "0";

            switch (std::stoi(choice)) {
            case 0: running = false; break;
            case 1: Executor::passthrough("nmtui 2>/dev/null || true"); break;
            case 2: enable_network_device(); break;
            case 3: wifi_scan(); break;
            case 4: network_device_status(); break;
            case 5: install_network_driver(); break;
            case 6: view_ip_address(); break;
            case 7: install_wifi_qr_tool(); break;
            case 8: edit_network_config(); break;
            case 9: toggle_nm_autostart(); break;
            case 10: install_blueman_pkg(); break;
            case 11: install_gnome_nettool_pkg(); break;
            default: running = false; break;
            }
            if (running) Logger::press_enter();
        }
    }

    void BetaFeaturesManager::enable_network_device() {
        // 列出网络设备
        auto devs = Executor::shell(
            "nmcli d 2>/dev/null | grep -Ev '^lo|^DEVICE' | awk '{print $1}'");
        std::string dlist = devs.ok() ? devs.stdout_data : "";
        if (dlist.empty()) {
            Logger::warn("No network devices found");
            return;
        }

        std::string dmenu = cfg_.tui_bin +
            " --title \"NETWORK DEVICES\" --menu \""
            "Which network device do you want to enable?\" 0 0 0 ";
        int n = 1;
        std::vector<std::string> devices;
        for (auto& d : split(dlist, '\n')) {
            if (d.empty()) continue;
            d.erase(std::remove(d.begin(), d.end(), '\r'), d.end());
            devices.push_back(d);
            dmenu += "\"" + std::to_string(n++) + "\" \"" + d + "\" ";
        }
        dmenu += "\"0\" \"Back / 返回\"";

        std::string pick = Executor::tui_select(dmenu);
        if (pick == "0" || pick.empty()) return;
        int idx = std::stoi(pick) - 1;
        if (idx < 0 || idx >= (int)devices.size()) return;

        std::string dev = devices[idx];
        if (Executor::shell("sudo ip link set " + dev + " up 2>/dev/null").ok()) {
            Logger::ok("Device " + dev + " enabled");
        } else {
            Logger::error("Failed to enable device " + dev);
        }
    }

    void BetaFeaturesManager::wifi_scan() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (!Executor::has("iw")) PackageManager::install("iw", family);
        if (!Executor::has("iwlist")) {
            PackageManager::install(
                cfg_.linux_distro == "arch" ? "wireless_tools" : "wireless-tools", family);
        }
        Logger::info("Scanning WiFi networks...");
        Executor::passthrough(
            "cd /tmp && iwlist scan 2>/dev/null | tee .tmoe_wifi_scan_cache; "
            "cat .tmoe_wifi_scan_cache | grep --color=auto -i 'SSID'; "
            "rm -f .tmoe_wifi_scan_cache");
    }

    void BetaFeaturesManager::network_device_status() {
        Executor::passthrough("iw phy 2>/dev/null || true");
        Logger::info("---");
        Executor::passthrough("nmcli device show 2>&1 | head -n 100 || true");
        Logger::info("---");
        Executor::passthrough("nmcli connection show 2>/dev/null || true");
        Logger::info("---");
        Executor::passthrough("iw dev 2>/dev/null || true");
        Logger::info("---");
        Executor::passthrough("nmcli radio 2>/dev/null || true");
        Logger::info("---");
        Executor::passthrough("nmcli device 2>/dev/null || true");
    }

    void BetaFeaturesManager::install_network_driver() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string drv = Executor::tui_select(
            cfg_.tui_bin + " --title \"NETWORK DRIVER\" --menu \""
            "Which driver do you want to install?\" 15 50 7 "
            "\"1\" \"list devices / 查看设备列表\" "
            "\"2\" \"Intel WiFi (firmware-iwlwifi)\" "
            "\"3\" \"Realtek (firmware-realtek)\" "
            "\"4\" \"Marvell (firmware-libertas)\" "
            "\"5\" \"TI Connectivity (firmware-ti-connectivity)\" "
            "\"6\" \"Broadcom (firmware-brcm80211)\" "
            "\"7\" \"misc / 其他\" "
            "\"0\" \"Back / 返回\" "
            "3>&1 1>&2 2>&3");

        std::string pkg;
        bool list_only = false;
        switch (drv.empty() ? 0 : std::stoi(drv)) {
        case 0: return;
        case 1:
            if (!Executor::has("dmidecode")) PackageManager::install("dmidecode", family);
            Executor::passthrough("dmidecode 2>/dev/null | grep --color=auto -Ei 'Wire|Net' || true");
            return;
        case 2: pkg = "firmware-iwlwifi"; break;
        case 3: pkg = "firmware-realtek"; break;
        case 4: pkg = "firmware-libertas"; break;
        case 5: pkg = "firmware-ti-connectivity"; break;
        case 6: pkg = "firmware-brcm80211"; break;
        case 7: pkg = "firmware-misc-nonfree"; break;
        default: return;
        }

        int act = ui::dialog::yesno(cfg_, "", "Install or download?",
            "INSTALL/安装", "DOWNLOAD/下载", 8, 50);
        if (act == 0) {
            PackageManager::install(pkg, family);
        } else {
            Logger::step("Downloading " + pkg + "...");
            Executor::passthrough(
                "mkdir -pv ~/sd/Download && cd ~/sd/Download && "
                "apt download " + pkg + " 2>/dev/null || "
                "aria2c --console-log-level=warn --no-conf --allow-overwrite=true "
                "-s 5 -x 5 -k 1M "
                "'https://mirrors.bfsu.edu.cn/debian/pool/non-free/f/firmware-nonfree/'"
                "$(curl -sL 'https://mirrors.bfsu.edu.cn/debian/pool/non-free/f/firmware-nonfree/' | "
                "grep '\\.deb' | grep '" + pkg + "' | tail -n 1 | cut -d'=' -f3 | cut -d'\"' -f2)");
            Logger::ok("Downloaded to ~/sd/Download");
        }
    }

    void BetaFeaturesManager::view_ip_address() {
        Executor::passthrough("ip a 2>/dev/null || true");
        Executor::passthrough("ip -br -c a 2>/dev/null || true");
        Executor::passthrough(
            "curl -L myip.ipip.net 2>/dev/null || curl -L ip.cip.cc 2>/dev/null || true");
    }

    void BetaFeaturesManager::install_wifi_qr_tool() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (!Executor::has("wifi-qr")) {
            PackageManager::install("wifi-qr", family);
        }
        Logger::info("Type wifi-qr to start it.");
        Executor::passthrough("wifi-qr t 2>/dev/null || true");
    }

    void BetaFeaturesManager::edit_network_config() {
        std::string editor = "vi";
        for (const auto* e : {"editor", "micro", "nano", "vim", "vi"}) {
            if (Executor::has(e)) { editor = e; break; }
        }
        Executor::passthrough(editor + " /etc/NetworkManager/system-connections/* 2>/dev/null || true");
        Executor::passthrough(editor + " /etc/NetworkManager/NetworkManager.conf 2>/dev/null || true");
        Executor::passthrough(editor + " /etc/network/interfaces.d/* 2>/dev/null || true");
        Executor::passthrough(editor + " /etc/network/interfaces 2>/dev/null || true");
    }

    void BetaFeaturesManager::toggle_nm_autostart() {
        bool is_alpine = (cfg_.linux_distro == "alpine");
        std::string svc = is_alpine ? "networkmanager" : "NetworkManager";

        int choice = ui::dialog::yesno(cfg_, "",
            "Enable or disable NetworkManager on boot?",
            "ENABLE/启用", "DISABLE/禁用", 0, 50);
        if (choice == 0) {
            if (is_alpine) {
                Executor::passthrough("sudo rc-update add " + svc + " 2>/dev/null || true");
            } else {
                Executor::passthrough("sudo systemctl enable " + svc + " 2>/dev/null || true");
            }
            Logger::ok("NetworkManager enabled on boot");
        } else if (choice == 1) {
            if (is_alpine) {
                Executor::passthrough("sudo rc-update del " + svc + " 2>/dev/null || true");
            } else {
                Executor::passthrough("sudo systemctl disable " + svc + " 2>/dev/null || true");
            }
            Logger::ok("NetworkManager disabled on boot");
        }
    }

    void BetaFeaturesManager::install_blueman_pkg() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string pkg1 = (cfg_.linux_distro == "alpine") ? "gnome-bluetooth" : "blueman-manager";
        PackageManager::install({pkg1, "blueman"}, family);
    }

    void BetaFeaturesManager::install_gnome_nettool_pkg() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string pkg2 = (cfg_.linux_distro == "debian") ? "network-manager-gnome" : "gnome-network-manager";
        PackageManager::install({"gnome-nettool", pkg2}, family);
    }

    // ═══════════════════════════════════════════════════════════════
    //  辅助函数
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::install_single(const std::string &i18n_key,
                                             const std::string &pkg) {
        Logger::step(_(i18n_key));
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install(pkg, family);
    }

    void BetaFeaturesManager::install_multi(const std::vector<std::string> &pkgs) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install(pkgs, family);
    }

} // namespace tmoe::domain
