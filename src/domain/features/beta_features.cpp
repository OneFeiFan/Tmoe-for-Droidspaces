#include "domain/features/beta_features.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include <algorithm>
#include <sstream>

namespace tmoe::domain {
    BetaFeaturesManager::BetaFeaturesManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项9: 网络管理 (对应 Bash: network 321行) — 占位
    // ═══════════════════════════════════════════════════════════════

    // TODO(network): 完整网络管理 — 对应 Bash old/tools/system/network (321行)
    //   - nmtui: 检查并安装 network-manager, 检查 managed=true, 启动 NetworkManager
    //   - enable device: nmcli device list → ip link set <dev> up
    //   - WiFi scan: 安装 iw+wireless-tools, iwlist scan, 解析 SSID
    //   - device status: iw phy, nmcli device show, nmcli connection show
    //   - driver: check_debian_nonfree_source → firmware-iwlwifi/realtek/libertas/brcm/misc
    //   - view IP: ip -br -c a, curl myip.ipip.net / ip.cip.cc
    //   - wifi-qr: 安装 wifi-qr, wifi-qr t
    //   - edit config: 编辑器遍历 → /etc/NetworkManager/system-connections/* 等
    //   - systemctl enable: alpine→rc-update, 其他→systemctl enable
    //   - blueman: alpine→gnome-bluetooth+blueman, 其他→blueman-manager+blueman
    //   - gnome-nettool: debian→network-manager-gnome, 其他→gnome-network-manager
    void BetaFeaturesManager::run_network_menu() {
        Logger::warn(_("misc.not_implemented"));
        Logger::info(_("beta.net_not_implemented"));
        Logger::info(_("beta.net_will_cover"));
        Logger::info(_("beta.net_will_cover2"));
        Logger::info(_("beta.net_will_cover3"));
        Logger::info("---");
        Logger::info(_("beta.net_use_directly"));
        Logger::info("  nmtui              — NetworkManager TUI");
        Logger::info("  nmcli device wifi   — WiFi scan & connect");
        Logger::info("  ip -br -c a        — view IP addresses");
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
