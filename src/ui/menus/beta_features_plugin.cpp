/** BetaFeatures 菜单插件 — 将 run_beta_menu() 的 12 个子项展平为独立 LambdaAction。
 * Items 3-9,12 使用 MenuEngine + build_*_menu() 构建新的 IUIMenu 树。
 * Items 1,2,10,11 通过 BetaFeaturesManager 跨模块委托方法调用。
 * 每个 build_*_menu() 函数将原先的 whiptail switch-case 逐条映射为 LambdaAction。 */

#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/features/beta_features.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/executor.h"
#include "core/logger.h"
#include <algorithm>
#include <sstream>

namespace tmoe::ui::menus {

// ─────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────

static std::shared_ptr<IUIMenu> build_system_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_uefi_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_store_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_deepin_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_video_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_paint_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_r_lang_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_file_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_file_manager_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_reader_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_other_menu(domain::BetaFeaturesManager* mgr);
static std::shared_ptr<IUIMenu> build_scrcpy_menu(domain::BetaFeaturesManager* mgr);

// ─────────────────────────────────────────────────────────────────────
//  Top-level factory: 12 items → create_beta_features_menu()
// ─────────────────────────────────────────────────────────────────────

std::shared_ptr<IUIMenu> create_beta_features_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.menu_title"), _("beta.menu_prompt"), "plugin_beta_features");

    // 1: container/vm → delegates to VirtualizationManager
    menu->add_child(LambdaAction::make(
        _("beta.opt1_virt"), "1",
        [mgr] { mgr->virt_delegate(); }));

    // 2: science & education → delegates to EducationManager
    menu->add_child(LambdaAction::make(
        _("beta.opt2_science"), "2",
        [mgr] { mgr->education_delegate(); }));

    // 3: system management → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt3_system"), "3",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_system_menu(mgr));
            return true;
        }));

    // 4: store & download → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt4_store"), "4",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_store_menu(mgr));
            return true;
        }));

    // 5: video editing → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt5_video"), "5",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_video_menu(mgr));
            return true;
        }));

    // 6: paint → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt6_paint"), "6",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_paint_menu(mgr));
            return true;
        }));

    // 7: file management → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt7_file"), "7",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_file_menu(mgr));
            return true;
        }));

    // 8: reader → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt8_read"), "8",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_reader_menu(mgr));
            return true;
        }));

    // 9: network — calls old placeholder method
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt9_network"), "9",
        [mgr](MenuContext&) -> bool {
            mgr->run_network_menu();
            Logger::press_enter();
            return true;
        }));

    // 10: input method → delegates to InputMethodManager
    menu->add_child(LambdaAction::make(
        _("beta.opt10_input"), "10",
        [mgr] { mgr->input_method_delegate(); }));

    // 11: terminal → delegates to TerminalAppManager
    menu->add_child(LambdaAction::make(
        _("beta.opt11_terminal"), "11",
        [mgr] { mgr->terminal_delegate(); }));

    // 12: other → MenuEngine nested submenu
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.opt12_other"), "12",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_other_menu(mgr));
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_system_menu — 10 items (对应 Bash sys-menu 406行)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_system_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.sys_title"), _("beta.sys_prompt"), "plugin_beta_system");

    // ── 1. sudo 用户组管理 (对应 Bash tmoe_linux_sudo_user_group_management) ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_sudo_mgr"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto users_out = Executor::shell(
                "grep -Ev 'nologin|halt|shutdown|0:0' /etc/passwd | awk -F':' '{print $1}'");
            std::string user_list = users_out.ok() ? users_out.stdout_data : "";
            if (user_list.empty()) {
                Logger::error(_("beta.sys_no_users"));
                Logger::info(_("beta.sys_no_users_detail"));
                Logger::press_enter();
                return true;
            }
            std::string umenu = ctx.cfg.tui_bin +
                " --title \"USER LIST\" --menu \"Which user to add/remove from sudo group?\" 0 0 0 ";
            int n = 1;
            std::vector<std::string> users;
            std::istringstream iss(user_list);
            std::string u;
            while (std::getline(iss, u)) {
                if (u.empty()) continue;
                u.erase(std::remove(u.begin(), u.end(), '\r'), u.end());
                users.push_back(u);
                umenu += "\"" + std::to_string(n++) + "\" \"" + u + "\" ";
            }
            umenu += "\"0\" \"" + _("menu.tui.back") + "\"";
            std::string pick = Executor::tui_select(umenu);
            if (pick == "0" || pick.empty()) { Logger::press_enter(); return true; }
            int idx = std::stoi(pick) - 1;
            if (idx < 0 || idx >= (int)users.size()) { Logger::press_enter(); return true; }
            std::string chosen = users[idx];

            bool in_sudo = CommandBuilder("grep").add_flag("-q")
                               .add_arg("^" + chosen + ".*ALL")
                               .add_arg("/etc/sudoers")
                               .add_raw("2>/dev/null").execute().ok() ||
                Executor::shell(
                    "grep sudo /etc/group 2>/dev/null | grep -q '" + chosen + "'").ok();
            if (in_sudo) {
                Logger::info(_f("beta.sys_sudo_in_group", chosen));
                if (ctx.cfg.linux_distro == "debian")
                    CommandBuilder("sudo").add_arg("deluser").add_arg(chosen).add_arg("sudo").add_raw("2>/dev/null").execute();
                CommandBuilder("sudo").add_arg("sed").add_flag("-i")
                    .add_arg("/^" + chosen + ".*ALL/d")
                    .add_arg("/etc/sudoers")
                    .add_raw("2>/dev/null").execute();
                Logger::ok(_f("beta.sys_sudo_removed", chosen));
            } else {
                CommandBuilder("sudo").add_arg("sed").add_flag("-i")
                    .add_arg("/^root.*ALL/a " + chosen + "    ALL=(ALL:ALL) ALL")
                    .add_arg("/etc/sudoers")
                    .add_raw("2>/dev/null").execute();
                Logger::ok(_f("beta.sys_sudo_added", chosen));
            }
            Logger::press_enter();
            return true;
        }));

    // ── 2. rc.local systemd (对应 Bash modify_rc_local_script) ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_rc_local"), "2",
        [mgr](MenuContext&) -> bool {
            if (!Executor::shell("test -f /etc/rc.local").ok()) {
                Executor::shell(
                    "sudo cat >/etc/rc.local <<'EOF'\n#!/bin/sh -e\n# rc.local\n# Add your startup commands above exit 0\nexit 0\nEOF\n");
                Executor::shell("sudo chmod a+rx /etc/rc.local");
            }
            if (!Executor::shell("test -f /etc/systemd/system/rc-local.service").ok()) {
                Executor::shell(
                    "sudo tee /etc/systemd/system/rc-local.service >/dev/null <<'EOF'\n[Unit]\nDescription=/etc/rc.local\nConditionPathExists=/etc/rc.local\n[Service]\nType=forking\nExecStart=/etc/rc.local start\nTimeoutSec=0\nStandardOutput=tty\nRemainAfterExit=yes\nSysVStartPriority=99\n[Install]\nWantedBy=multi-user.target\nEOF\n");
            }
            Executor::passthrough("nano /etc/rc.local 2>/dev/null || vi /etc/rc.local");
            Executor::shell("sudo systemctl daemon-reload 2>/dev/null");
            Executor::shell("sudo systemctl enable rc-local.service 2>/dev/null");
            Logger::ok(_("beta.sys_rc_local_ok"));
            Logger::press_enter();
            return true;
        }));

    // ── 3. UEFI 启动管理 → 嵌套子菜单 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_uefi_boot"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            if (!Executor::has("efibootmgr"))
                domain::PackageManager::install("efibootmgr", family);
            if (!Executor::has("efibootmgr")) {
                Logger::error(_("beta.sys_efi_install_failed"));
                Logger::info("apt install efibootmgr   # Debian/Ubuntu");
                Logger::info("pacman -S efibootmgr    # Arch");
                Logger::info("dnf install efibootmgr  # Fedora");
                Logger::press_enter();
                return true;
            }
            MenuEngine(ctx).run(build_uefi_menu(mgr));
            return true;
        }));

    // ── 4. gnome-system-monitor ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_gnome_monitor"), "4",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("gnome-system-monitor", family);
            Logger::press_enter();
            return true;
        }));

    // ── 5. grub-customizer ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_grub_customizer"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("grub-customizer", family);
            Logger::press_enter();
            return true;
        }));

    // ── 6. gnome-system-tools + gnome-logs ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_gnome_logs"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"gnome-system-tools", "gnome-logs"}, family);
            Logger::press_enter();
            return true;
        }));

    // ── 7. boot-repair (Debian only) ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_boot_repair"), "7",
        [mgr](MenuContext& ctx) -> bool {
            if (ctx.cfg.linux_distro != "debian") {
                Logger::error(_("beta.sys_bootrepair_debian_only"));
                Logger::info(_("beta.sys_bootrepair_deb_msg"));
                Logger::info("Manual: https://help.ubuntu.com/community/Boot-Repair");
                Logger::info("For other distros, try: grub-mkconfig -o /boot/grub/grub.cfg");
                Logger::press_enter();
                return true;
            }
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::update(family);
            domain::PackageManager::install("software-properties-common", family);
            Executor::passthrough("sudo add-apt-repository -y ppa:yannubuntu/boot-repair 2>/dev/null");
            domain::PackageManager::update(family);
            domain::PackageManager::install("boot-repair", family);
            Logger::ok(_("beta.sys_bootrepair_installed"));
            Logger::press_enter();
            return true;
        }));

    // ── 8. neofetch ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_neofetch"), "8",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            if (!Executor::has("neofetch")) {
                domain::PackageManager::install("neofetch", family);
            }
            if (!Executor::has("neofetch")) {
                Executor::passthrough(
                    "sudo curl -L -o /usr/local/bin/neofetch "
                    "'https://gitee.com/mirrors/neofetch/raw/master/neofetch' 2>/dev/null");
                Executor::shell("sudo chmod a+rx /usr/local/bin/neofetch");
            }
            if (Executor::has("lolcat") || Executor::shell("test -f /usr/games/lolcat").ok())
                Executor::passthrough("neofetch | /usr/games/lolcat 2>/dev/null || neofetch | lolcat 2>/dev/null || neofetch");
            else
                Executor::passthrough("neofetch");
            Logger::press_enter();
            return true;
        }));

    // ── 9. yasat ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_yasat"), "9",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            if (!Executor::has("yasat"))
                domain::PackageManager::install("yasat", family);
            if (Executor::has("yasat"))
                Executor::passthrough("yasat --full-scan");
            else {
                Logger::error(_("beta.sys_yasat_install_failed"));
                Logger::info("apt install yasat    # Debian/Ubuntu");
                Logger::info("pip install yasat    # pip fallback");
                Logger::info("Manual: https://github.com/gh0st-arch/yasat");
            }
            Logger::press_enter();
            return true;
        }));

    // ── 10. Tmoe-linux manager (旧版外部脚本) ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.sys_tmoe_manager"), "10",
        [mgr](MenuContext&) -> bool {
            Logger::info(_("beta.sys_old_manager"));
            Logger::info(_("beta.sys_old_manager_desc"));
            Logger::info("  ${TMOE_GIT_DIR}/share/old-version/share/app/manager");
            Logger::info("Run: bash /path/to/tmoe-linux/share/old-version/share/app/manager");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_uefi_menu — 5 items (UEFI 启动管理子菜单)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_uefi_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.uefi_title"), "", "plugin_beta_uefi");

    // 1: 修改第一启动项
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.uefi_modify_first"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto out = Executor::shell("efibootmgr 2>/dev/null");
            Logger::info(out.ok() ? out.stdout_data : _("beta.sys_efibootmgr_failed"));
            std::string input = Executor::tui_select(
                ctx.cfg.tui_bin + " --title \"" + _("beta.uefi_boot_item_title") + "\" --inputbox \"" + _("beta.uefi_boot_num_prompt") + "\" 0 0");
            if (!input.empty())
                CommandBuilder("sudo").add_arg("efibootmgr").add_flag("-o").add_arg(input).add_raw("2>/dev/null").execute();
            Logger::press_enter();
            return true;
        }));

    // 2: 自定义启动顺序
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.uefi_custom_order"), "2",
        [mgr](MenuContext& ctx) -> bool {
            std::string order = Executor::tui_select(
                ctx.cfg.tui_bin + " --title \"" + _("beta.uefi_boot_order_title") + "\" --inputbox \"" + _("beta.uefi_boot_order_prompt") + "\" 0 0");
            if (!order.empty())
                CommandBuilder("sudo").add_arg("efibootmgr").add_flag("-o").add_arg(order).add_raw("2>/dev/null").execute();
            Logger::press_enter();
            return true;
        }));

    // 3: 备份 EFI
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.uefi_backup_efi"), "3",
        [mgr](MenuContext&) -> bool {
            std::string efi_disk = Executor::shell(
                "df -h | grep '/boot/efi' | awk '{print $1}' | head -n1").stdout_data;
            efi_disk.erase(std::remove(efi_disk.begin(), efi_disk.end(), '\n'), efi_disk.end());
            if (!efi_disk.empty())
                CommandBuilder("sudo").add_arg("dd").add_arg("if=" + efi_disk).add_arg("of=/tmp/efi_backup.img").add_arg("bs=4M").add_raw("2>/dev/null").execute();
            Logger::ok(_("beta.sys_efi_backed_up"));
            Logger::press_enter();
            return true;
        }));

    // 4: 恢复 EFI
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.uefi_restore_efi"), "4",
        [mgr](MenuContext&) -> bool {
            Executor::passthrough("sudo dd if=/tmp/efi_backup.img of=$(df -h | grep '/boot/efi' | awk '{print $1}' | head -n1) bs=4M 2>/dev/null");
            Logger::ok(_("beta.sys_efi_restored"));
            Logger::press_enter();
            return true;
        }));

    // 5: 安装 rEFInd
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.uefi_install_refind"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"refind", "refind-install"}, family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_store_menu — 8 items (对应 Bash beta_features:217-250)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_store_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.store_title"), _("beta.store_prompt"), "plugin_beta_store");

    // 1: aptitude
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_aptitude"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("aptitude", family);
            if (Executor::has("aptitude")) {
                Executor::passthrough("aptitude");
            } else {
                Logger::error(_("beta.store_aptitude_debian_only"));
                Logger::info(_("beta.store_aptitude_deb_only_msg"));
                Logger::info(_("beta.store_aptitude_alternatives"));
            }
            Logger::press_enter();
            return true;
        }));

    // 2: deepin → 嵌套子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_deepin"), "2",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_deepin_menu(mgr));
            return true;
        }));

    // 3: gnome-software
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_gnome_sw"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("gnome-software", family);
            Logger::press_enter();
            return true;
        }));

    // 4: plasma-discover / discover
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_plasma"), "4",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            std::string pkg = (ctx.cfg.linux_distro == "arch") ? "discover" : "plasma-discover";
            domain::PackageManager::install(pkg, family);
            Logger::press_enter();
            return true;
        }));

    // 5: Flatpak + Flathub
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_flatpak"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"flatpak", "gnome-software-plugin-flatpak"}, family);
            Executor::passthrough(
                "flatpak remote-add --if-not-exists flathub "
                "https://flathub.org/repo/flathub.flatpakrepo 2>/dev/null || true");
            Logger::press_enter();
            return true;
        }));

    // 6: Snap
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_snap"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            if (ctx.cfg.linux_distro == "arch") {
                domain::PackageManager::install({"snapd", "snapd-xdg-open-git"}, family);
            } else {
                domain::PackageManager::install({"snapd", "gnome-software-plugin-snap"}, family);
            }
            Executor::passthrough("sudo snap install snap-store 2>/dev/null || true");
            Logger::press_enter();
            return true;
        }));

    // 7: bauh (pip)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_bauh"), "7",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            std::string pip_pkg;
            if (ctx.cfg.linux_distro == "alpine") pip_pkg = "py3-pip";
            else if (ctx.cfg.linux_distro == "debian" || ctx.cfg.linux_distro == "redhat")
                pip_pkg = "python3-pip";
            else pip_pkg = "python-pip";
            domain::PackageManager::install(pip_pkg, family);
            Executor::passthrough("sudo -H pip3 install bauh 2>/dev/null || "
                "su -c \"pip3 install bauh\" 2>/dev/null || true");
            Logger::press_enter();
            return true;
        }));

    // 8: qbittorrent
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.store_qbittorrent"), "8",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("qbittorrent", family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_deepin_menu — 16 items (对应 Bash beta_features:252-300)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_deepin_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.deepin_title"), _("beta.deepin_prompt"), "plugin_beta_deepin");

    static const std::string pkgs[] = {
        "dde-calendar", "dde-qt5integration", "deepin-calculator",
        "deepin-deb-installer", "deepin-gettext-tools", "deepin-image-viewer",
        "deepin-menu", "deepin-movie", "deepin-music",
        "deepin-notifications", "deepin-picker", "deepin-screen-recorder",
        "deepin-screenshot", "deepin-shortcut-viewer", "deepin-terminal",
        "deepin-voice-recorder"
    };

    static const char* labels[] = {
        "beta.deepin_calendar", "beta.deepin_qt5integration", "beta.deepin_calculator",
        "beta.deepin_deb_installer", "beta.deepin_gettext_tools", "beta.deepin_image_viewer",
        "beta.deepin_menu_service", "beta.deepin_movie", "beta.deepin_music",
        "beta.deepin_notifications", "beta.deepin_picker", "beta.deepin_screen_recorder",
        "beta.deepin_screenshot", "beta.deepin_shortcut_viewer", "beta.deepin_terminal",
        "beta.deepin_voice_recorder"
    };

    for (int i = 0; i < 16; ++i) {
        std::string tag = std::to_string(i + 1);
        std::string pkg = pkgs[i];
        std::string label = _(labels[i]);
        menu->add_child(std::make_shared<LambdaAction>(
            label, tag,
            [mgr, pkg](MenuContext& ctx) -> bool {
                auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
                domain::PackageManager::install(pkg, family);
                Logger::press_enter();
                return true;
            }));
    }

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_video_menu — 7 items (对应 Bash beta_features:603-634)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_video_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.video_title"), _("beta.video_prompt"), "plugin_beta_video");

    static const std::string pkgs[] = {
        "openshot", "mkvtoolnix-gui", "kdenlive",
        "flowblade", "shotcut", "olive-editor", "blender"
    };

    static const char* labels[] = {
        "beta.video_openshot", "beta.video_mkvtoolnix", "beta.video_kdenlive",
        "beta.video_flowblade", "beta.video_shotcut", "beta.video_olive",
        "beta.video_blender"
    };

    for (int i = 0; i < 7; ++i) {
        std::string tag = std::to_string(i + 1);
        std::string pkg = pkgs[i];
        std::string label = _(labels[i]);
        menu->add_child(std::make_shared<LambdaAction>(
            label, tag,
            [mgr, pkg](MenuContext& ctx) -> bool {
                auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
                domain::PackageManager::install(pkg, family);
                Logger::press_enter();
                return true;
            }));
    }

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_paint_menu — 11 items (对应 Bash beta_features:343-395)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_paint_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.paint_title"), _("beta.paint_prompt"), "plugin_beta_paint");

    // 1: krita
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_krita"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"krita", "krita-l10n"}, family);
            Logger::press_enter();
            return true;
        }));

    // 2: inkscape
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_inkscape"), "2",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"inkscape-tutorials", "inkscape"}, family);
            Logger::press_enter();
            return true;
        }));

    // 3: kolourpaint
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_kolourpaint"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("kolourpaint", family);
            Logger::press_enter();
            return true;
        }));

    // 4: R语言 → 嵌套子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_r_lang"), "4",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_r_lang_menu(mgr));
            return true;
        }));

    // 5: latexdraw
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_latexdraw"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("latexdraw", family);
            Logger::press_enter();
            return true;
        }));

    // 6: librecad
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_librecad"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("librecad", family);
            Logger::press_enter();
            return true;
        }));

    // 7: freecad
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_freecad"), "7",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("freecad", family);
            Logger::press_enter();
            return true;
        }));

    // 8: opencad
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_opencad"), "8",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("opencad", family);
            Logger::press_enter();
            return true;
        }));

    // 9: kicad
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_kicad"), "9",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"kicad-templates", "kicad"}, family);
            Logger::press_enter();
            return true;
        }));

    // 10: openscad
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_openscad"), "10",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("openscad", family);
            Logger::press_enter();
            return true;
        }));

    // 11: gnuplot
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.paint_gnuplot"), "11",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"gnuplot", "gnuplot-x11"}, family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_r_lang_menu — 3 items (对应 Bash beta_features:397-420)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_r_lang_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.r_title"), _("beta.r_prompt"), "plugin_beta_r_lang");

    // 1: r-base
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.r_base"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("r-base", family);
            Logger::press_enter();
            return true;
        }));

    // 2: RStudio (对应 Bash install_r_studio: amd64 only, distro-specific)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.rstudio"), "2",
        [mgr](MenuContext& ctx) -> bool {
            if (ctx.cfg.arch != "amd64") {
                Logger::error(_("beta.rstudio_amd64_only"));
                Logger::press_enter();
                return true;
            }
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            if (ctx.cfg.linux_distro == "arch") {
                domain::PackageManager::install("rstudio-desktop-git", family);
            } else if (ctx.cfg.linux_distro == "debian") {
                Logger::step(_("beta.rstudio_downloading"));
                auto ver = Executor::shell(
                    "curl -sL 'https://rstudio.com/products/rstudio/download/#download' 2>/dev/null | "
                    "grep -oP 'rstudio-[\\d.]+-amd64\\.deb' | head -n1");
                std::string deb = ver.ok() ? ver.stdout_data : "";
                if (!deb.empty()) {
                    deb.erase(std::remove(deb.begin(), deb.end(), '\n'), deb.end());
                    Executor::passthrough("curl -L -o /tmp/" + deb + " "
                        "'https://download1.rstudio.org/electron/focal/amd64/" + deb + "' 2>/dev/null");
                    Executor::passthrough("sudo apt install -y /tmp/" + deb + " 2>/dev/null");
                    Logger::ok(_("beta.rstudio_installed"));
                } else {
                    Logger::error(_("beta.rstudio_version_failed"));
                }
            } else if (ctx.cfg.linux_distro == "redhat") {
                auto ver = Executor::shell(
                    "curl -sL 'https://rstudio.com/products/rstudio/download/#download' 2>/dev/null | "
                    "grep -oP 'rstudio-[\\d.]+-x86_64\\.rpm' | head -n1");
                std::string rpm = ver.ok() ? ver.stdout_data : "";
                if (!rpm.empty()) {
                    rpm.erase(std::remove(rpm.begin(), rpm.end(), '\n'), rpm.end());
                    Executor::passthrough("curl -L -o /tmp/" + rpm + " "
                        "'https://download1.rstudio.org/electron/focal/amd64/" + rpm + "' 2>/dev/null");
                    Executor::passthrough("sudo yum install -y /tmp/" + rpm + " 2>/dev/null");
                    Logger::ok(_("beta.rstudio_installed"));
                } else {
                    Logger::error(_("beta.rstudio_version_failed"));
                }
            } else {
                Logger::warn(_("beta.rstudio_limited_distros"));
            }
            Logger::press_enter();
            return true;
        }));

    // 3: r-recommended
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.r_recommended"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("r-recommended", family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_file_menu — 9 items (对应 Bash beta_features:461-496)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_file_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.file_title"), _("beta.file_prompt"), "plugin_beta_file");

    // 1: 文件管理器选择器 → 嵌套子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_fm"), "1",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_file_manager_menu(mgr));
            return true;
        }));

    // 2: catfish
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_catfish"), "2",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("catfish", family);
            Logger::press_enter();
            return true;
        }));

    // 3: gparted
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_gparted"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("gparted", family);
            Logger::press_enter();
            return true;
        }));

    // 4: baobab
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_baobab"), "4",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("baobab", family);
            Logger::press_enter();
            return true;
        }));

    // 5: cfdisk (util-linux)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_cfdisk"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("util-linux", family);
            Logger::press_enter();
            return true;
        }));

    // 6: partitionmanager
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_partitionmgr"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("partitionmanager", family);
            Logger::press_enter();
            return true;
        }));

    // 7: mc
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_mc"), "7",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("mc", family);
            Logger::press_enter();
            return true;
        }));

    // 8: ranger
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_ranger"), "8",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("ranger", family);
            Logger::press_enter();
            return true;
        }));

    // 9: gnome-disk-utility
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_gnome_disks"), "9",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("gnome-disk-utility", family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_file_manager_menu — 4 items (文件管理器选择器)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_file_manager_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.file_fm_title"), _("beta.file_fm_prompt"), "plugin_beta_fm_chooser");

    // 1: Thunar
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_fm_thunar"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("thunar", family);
            Logger::press_enter();
            return true;
        }));

    // 2: Nautilus
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_fm_nautilus"), "2",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("nautilus", family);
            Logger::press_enter();
            return true;
        }));

    // 3: Dolphin
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_fm_dolphin"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("dolphin", family);
            Logger::press_enter();
            return true;
        }));

    // 4: Thunar + Nautilus
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.file_fm_thunar_nautilus"), "4",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"thunar", "nautilus"}, family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_reader_menu — 8 items (对应 Bash beta_features:570-601)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_reader_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.reader_title"), _("beta.reader_prompt"), "plugin_beta_reader");

    // 1: calibre
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_calibre"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("calibre", family);
            Logger::press_enter();
            return true;
        }));

    // 2: fbreader
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_fbreader"), "2",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("fbreader", family);
            Logger::press_enter();
            return true;
        }));

    // 3: typora
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_typora"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("typora", family);
            Logger::press_enter();
            return true;
        }));

    // 4: xournal
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_xournal"), "4",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("xournal", family);
            Logger::press_enter();
            return true;
        }));

    // 5: evince
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_evince"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("evince", family);
            Logger::press_enter();
            return true;
        }));

    // 6: okular + extra backends
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_okular"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"okular", "okular-extra-backends"}, family);
            Logger::press_enter();
            return true;
        }));

    // 7: kchmviewer
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_kchmviewer"), "7",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("kchmviewer", family);
            Logger::press_enter();
            return true;
        }));

    // 8: pdfchain
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.reader_pdfchain"), "8",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("pdfchain", family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_other_menu — 6 items (对应 Bash beta_features:50-78)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_other_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        _("beta.other_title"), _("beta.other_prompt"), "plugin_beta_other");

    // 1: OBS Studio
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_obs"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            std::string obs_pkg = (ctx.cfg.linux_distro == "gentoo")
                                      ? "media-video/obs-studio"
                                      : "obs-studio";
            domain::PackageManager::install(obs_pkg, family);
            Logger::press_enter();
            return true;
        }));

    // 2: seahorse
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_seahorse"), "2",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("seahorse", family);
            Logger::press_enter();
            return true;
        }));

    // 3: kodi
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_kodi"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install({"kodi", "kodi-wayland"}, family);
            Logger::press_enter();
            return true;
        }));

    // 4: scrcpy → 嵌套子菜单
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_scrcpy"), "4",
        [mgr](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(build_scrcpy_menu(mgr));
            return true;
        }));

    // 5: flameshot
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_flameshot"), "5",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("flameshot", family);
            Logger::press_enter();
            return true;
        }));

    // 6: telegram-desktop
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.other_telegram"), "6",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("telegram-desktop", family);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ═════════════════════════════════════════════════════════════════════
//  build_scrcpy_menu — 5 items (对应 Bash beta_features:92-118)
// ═════════════════════════════════════════════════════════════════════

static std::shared_ptr<IUIMenu> build_scrcpy_menu(domain::BetaFeaturesManager* mgr) {
    auto menu = make_plugin_menu(
        "scrcpy", _("beta.scrcpy_prompt"), "plugin_beta_scrcpy");

    // 1: install scrcpy
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.scrcpy_install"), "1",
        [mgr](MenuContext& ctx) -> bool {
            auto family = domain::infer_family_from_config(ctx.cfg.linux_distro);
            domain::PackageManager::install("scrcpy", family);
            Logger::press_enter();
            return true;
        }));

    // 2: adb connect (对应 Bash scrcpy_connect_to_android_device)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.scrcpy_connect"), "2",
        [mgr](MenuContext& ctx) -> bool {
            std::string target = Executor::tui_select(
                ctx.cfg.tui_bin + " --title \"" + _("beta.scrcpy_adb_title") + "\""
                " --inputbox \"" + _("beta.scrcpy_adb_prompt") + "\" 0 0");
            if (target.empty()) target = "localhost:5555";
            if (target.find(':') == std::string::npos) target += ":5555";
            CommandBuilder("adb").add_arg("connect").add_arg(target).add_raw("2>/dev/null").execute();
            Executor::passthrough("adb devices -l 2>/dev/null");
            Logger::info(_("beta.scrcpy_start_prompt"));
            Logger::press_enter();
            return true;
        }));

    // 3: switch scrcpy device (对应 Bash switch_scrcpy_device)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.scrcpy_switch"), "3",
        [mgr](MenuContext& ctx) -> bool {
            auto devs = Executor::shell("adb devices 2>/dev/null | sed '1d;$d' | awk '{print $1}'");
            std::string dlist = devs.ok() ? devs.stdout_data : "";
            if (dlist.empty()) {
                Logger::warn(_("beta.scrcpy_no_devices"));
                Logger::press_enter();
                return true;
            }
            std::string dmenu = ctx.cfg.tui_bin +
                " --title \"" + _("beta.scrcpy_device_title") + "\" --menu \"" + _("beta.scrcpy_device_switch_prompt") + "\" 0 0 0 ";
            std::vector<std::string> devices;
            std::istringstream iss(dlist);
            std::string d;
            int dn = 1;
            while (std::getline(iss, d)) {
                if (d.empty()) continue;
                d.erase(std::remove(d.begin(), d.end(), '\r'), d.end());
                devices.push_back(d);
                dmenu += "\"" + std::to_string(dn++) + "\" \"" + d + "\" ";
            }
            dmenu += "\"0\" \"" + _("menu.tui.back") + "\"";
            std::string dpick = Executor::tui_select(dmenu);
            if (dpick == "0" || dpick.empty()) { Logger::press_enter(); return true; }
            int didx = std::stoi(dpick) - 1;
            if (didx >= 0 && didx < (int)devices.size())
                CommandBuilder("scrcpy").add_flag("-s").add_arg(devices[didx]).add_raw("2>/dev/null &").execute();
            Logger::press_enter();
            return true;
        }));

    // 4: restart adb
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.scrcpy_restart_adb"), "4",
        [mgr](MenuContext&) -> bool {
            Executor::passthrough("adb kill-server 2>/dev/null || true");
            Executor::passthrough("adb devices -l 2>/dev/null || true");
            Logger::press_enter();
            return true;
        }));

    // 5: scrcpy FAQ (对应 Bash scrpy_faq)
    menu->add_child(std::make_shared<LambdaAction>(
        _("beta.scrcpy_readme"), "5",
        [mgr](MenuContext&) -> bool {
            Logger::info(_("beta.scrcpy_faq_header"));
            Logger::info("  scrcpy            — start with defaults");
            Logger::info("  scrcpy -S         — turn off device screen");
            Logger::info("  scrcpy -m 1024    — limit resolution to 1024");
            Logger::info("  scrcpy -b 4M      — set video bitrate to 4M");
            Logger::info("  scrcpy -c 1920:1080:0:0 — crop display");
            Logger::info("  scrcpy -T         — keep window on top");
            Logger::info("  scrcpy -t         — show touch points");
            Logger::info("  scrcpy -f         — fullscreen");
            Logger::info("  scrcpy --push-target /path — file transfer dir");
            Logger::info("  scrcpy -n         — read-only (display only)");
            Logger::info("  scrcpy -r video.mp4 — screen recording");
            Logger::info("  scrcpy -Nr video.mkv — recording (no display)");
            Logger::info("  scrcpy --window-title 'title' — set window title");
            Logger::info("Docs: https://github.com/Genymobile/scrcpy");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
