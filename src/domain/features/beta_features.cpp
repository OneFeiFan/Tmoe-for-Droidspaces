#include "domain/features/beta_features.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"
#include <algorithm>
#include <sstream>

namespace tmoe::domain {
    BetaFeaturesManager::BetaFeaturesManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════════
    //  秘密花园主菜单 — 12 个子选项 (对应 Bash beta_features:3-42)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_beta_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.menu_title")
                               + "\" --menu \"" + _("beta.menu_prompt") + "\" 0 55 0 "
                               "\"1\" \"" + _("beta.opt1_virt") + "\" "
                               "\"2\" \"" + _("beta.opt2_science") + "\" "
                               "\"3\" \"" + _("beta.opt3_system") + "\" "
                               "\"4\" \"" + _("beta.opt4_store") + "\" "
                               "\"5\" \"" + _("beta.opt5_video") + "\" "
                               "\"6\" \"" + _("beta.opt6_paint") + "\" "
                               "\"7\" \"" + _("beta.opt7_file") + "\" "
                               "\"8\" \"" + _("beta.opt8_read") + "\" "
                               "\"9\" \"" + _("beta.opt9_network") + "\" "
                               "\"10\" \"" + _("beta.opt10_input") + "\" "
                               "\"11\" \"" + _("beta.opt11_terminal") + "\" "
                               "\"12\" \"" + _("beta.opt12_other") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") {
                // 容器/虚拟机 → 委托给 VirtualizationManager
                if (virt_cb_) virt_cb_();
            } else if (ch == "2") {
                // 科学与教育 → 委托给 EducationManager
                if (education_cb_) education_cb_();
            } else if (ch == "3") {
                run_system_menu();
            } else if (ch == "4") {
                run_store_menu();
            } else if (ch == "5") {
                run_video_menu();
            } else if (ch == "6") {
                run_paint_menu();
            } else if (ch == "7") {
                run_file_menu();
            } else if (ch == "8") {
                run_reader_menu();
            } else if (ch == "9") {
                run_network_menu();
            } else if (ch == "10") {
                // 输入法 → 委托给 InputMethodManager
                if (input_method_cb_) input_method_cb_();
            } else if (ch == "11") {
                // 终端 → 委托给 TerminalAppManager
                if (terminal_cb_) terminal_cb_();
            } else if (ch == "12") {
                run_other_menu();
            }
            // 不在末尾调用 press_enter() — 每个子菜单和委托回调内部已有自己的 enter 循环
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项3: 系统管理 (对应 Bash: sys-menu 406行)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_system_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.sys_title")
                               + "\" --menu \"" + _("beta.sys_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.sys_sudo_mgr") + "\" "
                               "\"2\" \"" + _("beta.sys_rc_local") + "\" "
                               "\"3\" \"" + _("beta.sys_uefi_boot") + "\" "
                               "\"4\" \"" + _("beta.sys_gnome_monitor") + "\" "
                               "\"5\" \"" + _("beta.sys_grub_customizer") + "\" "
                               "\"6\" \"" + _("beta.sys_gnome_logs") + "\" "
                               "\"7\" \"" + _("beta.sys_boot_repair") + "\" "
                               "\"8\" \"" + _("beta.sys_neofetch") + "\" "
                               "\"9\" \"" + _("beta.sys_yasat") + "\" "
                               "\"10\" \"" + _("beta.sys_tmoe_manager") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            auto family = infer_family_from_config(cfg_.linux_distro);

            switch (std::stoi(ch)) {
                // ── 1. sudo 用户组管理 (对应 Bash tmoe_linux_sudo_user_group_management) ──
                case 1: {
                    // 读取 /etc/passwd，排除 nologin/halt/shutdown/0:0
                    auto users_out = Executor::shell(
                        "grep -Ev 'nologin|halt|shutdown|0:0' /etc/passwd | awk -F':' '{print $1}'");
                    std::string user_list = users_out.ok() ? users_out.stdout_data : "";
                    if (user_list.empty()) {
                        Logger::error(_("beta.sys_no_users"));
                        Logger::info("No non-system users in /etc/passwd. Create a user first: adduser <name>");
                        break;
                    }

                    // 构建 whiptail 菜单
                    std::string umenu = cfg_.tui_bin +
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
                    if (pick == "0" || pick.empty()) break;
                    int idx = std::stoi(pick) - 1;
                    if (idx < 0 || idx >= (int)users.size()) break;
                    std::string chosen = users[idx];

                    // 检查是否已在 sudo 组
                    bool in_sudo = Executor::shell(
                        "grep -q '^" + chosen + ".*ALL' /etc/sudoers 2>/dev/null").ok() ||
                        Executor::shell(
                        "grep sudo /etc/group 2>/dev/null | grep -q '" + chosen + "'").ok();

                    if (in_sudo) {
                        // 移除
                        Logger::info(chosen + " is in sudo group. Remove?");
                        if (cfg_.linux_distro == "debian")
                            Executor::passthrough("deluser " + chosen + " sudo 2>/dev/null");
                        Executor::passthrough(
                            "sed -i '/^" + chosen + ".*ALL/d' /etc/sudoers 2>/dev/null");
                        Logger::ok(chosen + " removed from sudo");
                    } else {
                        // 添加
                        Executor::passthrough(
                            "sed -i '/^root.*ALL/a " + chosen + "    ALL=(ALL:ALL) ALL' /etc/sudoers 2>/dev/null");
                        Logger::ok(chosen + " added to sudo");
                    }
                    break;
                }

                // ── 2. rc.local systemd (对应 Bash modify_rc_local_script) ──
                case 2: {
                    if (!Executor::shell("test -f /etc/rc.local").ok()) {
                        Executor::shell(
                            "cat >/etc/rc.local <<'EOF'\n#!/bin/sh -e\n# rc.local\n# Add your startup commands above exit 0\nexit 0\nEOF\n");
                        Executor::shell("chmod a+rx /etc/rc.local");
                    }
                    if (!Executor::shell("test -f /etc/systemd/system/rc-local.service").ok()) {
                        Executor::shell(
                            "cat >/etc/systemd/system/rc-local.service <<'EOF'\n[Unit]\nDescription=/etc/rc.local\nConditionPathExists=/etc/rc.local\n[Service]\nType=forking\nExecStart=/etc/rc.local start\nTimeoutSec=0\nStandardOutput=tty\nRemainAfterExit=yes\nSysVStartPriority=99\n[Install]\nWantedBy=multi-user.target\nEOF\n");
                    }
                    Executor::passthrough("nano /etc/rc.local 2>/dev/null || vi /etc/rc.local");
                    Executor::shell("systemctl daemon-reload 2>/dev/null");
                    Executor::shell("systemctl enable rc-local.service 2>/dev/null");
                    Logger::ok("rc.local configured & enabled");
                    break;
                }

                // ── 3. UEFI 启动管理 (对应 Bash tmoe_uefi_boot_manager) ──
                case 3: {
                    if (!Executor::has("efibootmgr"))
                        PackageManager::install("efibootmgr", family);
                    if (!Executor::has("efibootmgr")) {
                        Logger::error(_("beta.sys_efi_install_failed"));
                        Logger::info("apt install efibootmgr   # Debian/Ubuntu");
                        Logger::info("pacman -S efibootmgr    # Arch");
                        Logger::info("dnf install efibootmgr  # Fedora");
                        break;
                    }

                    while (true) {
                        std::string bmenu = cfg_.tui_bin +
                            " --title \"UEFI BOOT MANAGER\" --menu \"\" 16 50 5 "
                            "\"1\" \"modify first boot item\" "
                            "\"2\" \"custom boot order\" "
                            "\"3\" \"backup EFI\" "
                            "\"4\" \"restore EFI\" "
                            "\"5\" \"install refind\" "
                            "\"0\" \"" + _("menu.tui.back") + "\"";
                        std::string bpick = Executor::tui_select(bmenu);
                        if (bpick == "0" || bpick.empty()) break;

                        if (bpick == "1") {
                            // 修改第一启动项
                            auto out = Executor::shell("efibootmgr 2>/dev/null");
                            Logger::info(out.ok() ? out.stdout_data : "efibootmgr failed");
                            std::string input = Executor::tui_select(
                                cfg_.tui_bin + " --title \"BOOT ITEM\" --inputbox \"Enter Boot number (e.g. 0001):\" 0 0");
                            if (!input.empty())
                                Executor::passthrough("efibootmgr -o " + input + " 2>/dev/null");
                        } else if (bpick == "2") {
                            std::string order = Executor::tui_select(
                                cfg_.tui_bin + " --title \"BOOT ORDER\" --inputbox \"Enter order (comma separated, e.g. 0001,0002):\" 0 0");
                            if (!order.empty())
                                Executor::passthrough("efibootmgr -o " + order + " 2>/dev/null");
                        } else if (bpick == "3") {
                            std::string efi_disk = Executor::shell(
                                "df -h | grep '/boot/efi' | awk '{print $1}' | head -n1").stdout_data;
                            efi_disk.erase(std::remove(efi_disk.begin(), efi_disk.end(), '\n'), efi_disk.end());
                            if (!efi_disk.empty())
                                Executor::passthrough("dd if=" + efi_disk + " of=/tmp/efi_backup.img bs=4M 2>/dev/null");
                            Logger::ok("EFI backed up to /tmp/efi_backup.img");
                        } else if (bpick == "4") {
                            Executor::passthrough("dd if=/tmp/efi_backup.img of=$(df -h | grep '/boot/efi' | awk '{print $1}' | head -n1) bs=4M 2>/dev/null");
                            Logger::ok("EFI restored");
                        } else if (bpick == "5") {
                            PackageManager::install({"refind", "refind-install"}, family);
                        }
                        Logger::press_enter();
                    }
                    break;
                }

                // ── 4~6: 已实现 ──
                case 4: PackageManager::install("gnome-system-monitor", family); break;
                case 5: PackageManager::install("grub-customizer", family);       break;
                case 6: PackageManager::install({"gnome-system-tools", "gnome-logs"}, family); break;

                // ── 7. boot-repair (对应 Bash install_boot_repair) ──
                case 7: {
                    if (cfg_.linux_distro != "debian") {
                        Logger::error(_("beta.sys_bootrepair_debian_only"));
                        Logger::info("boot-repair requires Ubuntu/Debian with PPA support.");
                        Logger::info("Manual: https://help.ubuntu.com/community/Boot-Repair");
                        Logger::info("For other distros, try: grub-mkconfig -o /boot/grub/grub.cfg");
                        break;
                    }
                    Executor::passthrough("apt update 2>/dev/null");
                    Executor::passthrough("apt install -y software-properties-common 2>/dev/null");
                    Executor::passthrough("add-apt-repository -y ppa:yannubuntu/boot-repair 2>/dev/null");
                    Executor::passthrough("apt update 2>/dev/null");
                    Executor::passthrough("apt install -y boot-repair 2>/dev/null");
                    Logger::ok("boot-repair installed");
                    break;
                }

                // ── 8. neofetch (对应 Bash start_neofetch) ──
                case 8: {
                    if (!Executor::has("neofetch")) {
                        if (cfg_.linux_distro == "debian")
                            Executor::passthrough("apt install -y --no-install-recommends neofetch 2>/dev/null");
                        else
                            PackageManager::install("neofetch", family);
                    }
                    // fallback: download from gitee
                    if (!Executor::has("neofetch")) {
                        Executor::passthrough(
                            "curl -L -o /usr/local/bin/neofetch "
                            "'https://gitee.com/mirrors/neofetch/raw/master/neofetch' 2>/dev/null");
                        Executor::shell("chmod a+rx /usr/local/bin/neofetch");
                    }
                    // run with lolcat if available
                    if (Executor::has("lolcat") || Executor::shell("test -f /usr/games/lolcat").ok())
                        Executor::passthrough("neofetch | /usr/games/lolcat 2>/dev/null || neofetch | lolcat 2>/dev/null || neofetch");
                    else
                        Executor::passthrough("neofetch");
                    break;
                }

                // ── 9. yasat (对应 Bash start_yasat) ──
                case 9: {
                    if (!Executor::has("yasat"))
                        PackageManager::install("yasat", family);
                    if (Executor::has("yasat"))
                        Executor::passthrough("yasat --full-scan");
                    else {
                        Logger::error(_("beta.sys_yasat_install_failed"));
                        Logger::info("apt install yasat    # Debian/Ubuntu");
                        Logger::info("pip install yasat    # pip fallback");
                        Logger::info("Manual: https://github.com/gh0st-arch/yasat");
                    }
                    break;
                }

                // ── 10. 保持占位 ──
                // ── 10. Tmoe-linux manager (旧版外部脚本, 对应 Bash source app/manager) ──
                case 10:
                    Logger::info(_("beta.sys_old_manager"));
                    Logger::info("This feature requires the legacy bash script from:");
                    Logger::info("  ${TMOE_GIT_DIR}/share/old-version/share/app/manager");
                    Logger::info("Run: bash /path/to/tmoe-linux/share/old-version/share/app/manager");
                    break;
                default: break;
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项4: 商店与下载 (对应 Bash: beta_features:217-250)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_store_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.store_title")
                               + "\" --menu \"" + _("beta.store_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.store_aptitude") + "\" "
                               "\"2\" \"" + _("beta.store_deepin") + "\" "
                               "\"3\" \"" + _("beta.store_gnome_sw") + "\" "
                               "\"4\" \"" + _("beta.store_plasma") + "\" "
                               "\"5\" \"" + _("beta.store_flatpak") + "\" "
                               "\"6\" \"" + _("beta.store_snap") + "\" "
                               "\"7\" \"" + _("beta.store_bauh") + "\" "
                               "\"8\" \"" + _("beta.store_qbittorrent") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            switch (std::stoi(ch)) {
                case 1:
                    // aptitude — 仅 Debian 系
                    if (cfg_.linux_distro == "debian") {
                        Executor::passthrough("aptitude");
                    } else {
                        Logger::error(_("beta.store_aptitude_debian_only"));
                        Logger::info("aptitude is only available on Debian-based distributions.");
                        Logger::info("Alternatives: pamac (Arch), dnfdragora (Fedora), yast (openSUSE)");
                    }
                    break;
                case 2: run_deepin_menu();
                    break;
                case 3: PackageManager::install("gnome-software", family);
                    break;
                case 4: {
                    std::string pkg = (cfg_.linux_distro == "arch") ? "discover" : "plasma-discover";
                    PackageManager::install(pkg, family);
                    break;
                }
                case 5:
                    // Flatpak + Flathub
                    PackageManager::install({"flatpak", "gnome-software-plugin-flatpak"}, family);
                    Executor::passthrough(
                        "flatpak remote-add --if-not-exists flathub "
                        "https://flathub.org/repo/flathub.flatpakrepo 2>/dev/null || true");
                    break;
                case 6:
                    // Snap
                    if (cfg_.linux_distro == "arch") {
                        PackageManager::install({"snapd", "snapd-xdg-open-git"}, family);
                    } else {
                        PackageManager::install({"snapd", "gnome-software-plugin-snap"}, family);
                    }
                    Executor::passthrough("snap install snap-store 2>/dev/null || true");
                    break;
                case 7:
                    // bauh — pip 安装
                {
                    std::string pip_pkg;
                    if (cfg_.linux_distro == "alpine") pip_pkg = "py3-pip";
                    else if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "redhat")
                        pip_pkg = "python3-pip";
                    else pip_pkg = "python-pip";
                    PackageManager::install(pip_pkg, family);
                    Executor::passthrough("sudo -H pip3 install bauh 2>/dev/null || "
                        "su -c \"pip3 install bauh\" 2>/dev/null || true");
                }
                break;
                case 8: PackageManager::install("qbittorrent", family);
                    break;
                default: break;
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  Deepin 子菜单 (对应 Bash: beta_features:252-300, 16项)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_deepin_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.deepin_title")
                               + "\" --menu \"" + _("beta.deepin_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.deepin_calendar") + "\" "
                               "\"2\" \"" + _("beta.deepin_qt5integration") + "\" "
                               "\"3\" \"" + _("beta.deepin_calculator") + "\" "
                               "\"4\" \"" + _("beta.deepin_deb_installer") + "\" "
                               "\"5\" \"" + _("beta.deepin_gettext_tools") + "\" "
                               "\"6\" \"" + _("beta.deepin_image_viewer") + "\" "
                               "\"7\" \"" + _("beta.deepin_menu_service") + "\" "
                               "\"8\" \"" + _("beta.deepin_movie") + "\" "
                               "\"9\" \"" + _("beta.deepin_music") + "\" "
                               "\"10\" \"" + _("beta.deepin_notifications") + "\" "
                               "\"11\" \"" + _("beta.deepin_picker") + "\" "
                               "\"12\" \"" + _("beta.deepin_screen_recorder") + "\" "
                               "\"13\" \"" + _("beta.deepin_screenshot") + "\" "
                               "\"14\" \"" + _("beta.deepin_shortcut_viewer") + "\" "
                               "\"15\" \"" + _("beta.deepin_terminal") + "\" "
                               "\"16\" \"" + _("beta.deepin_voice_recorder") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            // Deepin 包名列表 (与 bash 顺序一致)
            static const std::string pkgs[] = {
                "dde-calendar", "dde-qt5integration", "deepin-calculator",
                "deepin-deb-installer", "deepin-gettext-tools", "deepin-image-viewer",
                "deepin-menu", "deepin-movie", "deepin-music",
                "deepin-notifications", "deepin-picker", "deepin-screen-recorder",
                "deepin-screenshot", "deepin-shortcut-viewer", "deepin-terminal",
                "deepin-voice-recorder"
            };
            int idx = std::stoi(ch) - 1;
            if (idx >= 0 && idx < 16) {
                PackageManager::install(pkgs[idx], family);
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项5: 视频剪辑 (对应 Bash: beta_features:603-634)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_video_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.video_title")
                               + "\" --menu \"" + _("beta.video_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.video_openshot") + "\" "
                               "\"2\" \"" + _("beta.video_mkvtoolnix") + "\" "
                               "\"3\" \"" + _("beta.video_kdenlive") + "\" "
                               "\"4\" \"" + _("beta.video_flowblade") + "\" "
                               "\"5\" \"" + _("beta.video_shotcut") + "\" "
                               "\"6\" \"" + _("beta.video_olive") + "\" "
                               "\"7\" \"" + _("beta.video_blender") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            // 包名列表
            static const std::string pkgs[] = {
                "openshot", "mkvtoolnix-gui", "kdenlive",
                "flowblade", "shotcut", "olive-editor", "blender"
            };
            int idx = std::stoi(ch) - 1;
            if (idx >= 0 && idx < 7) {
                PackageManager::install(pkgs[idx], family);
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项6: 绘图 (对应 Bash: beta_features:343-395)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_paint_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.paint_title")
                               + "\" --menu \"" + _("beta.paint_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.paint_krita") + "\" "
                               "\"2\" \"" + _("beta.paint_inkscape") + "\" "
                               "\"3\" \"" + _("beta.paint_kolourpaint") + "\" "
                               "\"4\" \"" + _("beta.paint_r_lang") + "\" "
                               "\"5\" \"" + _("beta.paint_latexdraw") + "\" "
                               "\"6\" \"" + _("beta.paint_librecad") + "\" "
                               "\"7\" \"" + _("beta.paint_freecad") + "\" "
                               "\"8\" \"" + _("beta.paint_opencad") + "\" "
                               "\"9\" \"" + _("beta.paint_kicad") + "\" "
                               "\"10\" \"" + _("beta.paint_openscad") + "\" "
                               "\"11\" \"" + _("beta.paint_gnuplot") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            int choice = std::stoi(ch);
            switch (choice) {
                case 1:
                    PackageManager::install({"krita", "krita-l10n"}, family);
                    break;
                case 2:
                    PackageManager::install({"inkscape-tutorials", "inkscape"}, family);
                    break;
                case 3:
                    PackageManager::install("kolourpaint", family);
                    break;
                case 4:
                    run_r_lang_menu();
                    break;
                case 5:
                    PackageManager::install("latexdraw", family);
                    break;
                case 6:
                    PackageManager::install("librecad", family);
                    break;
                case 7:
                    PackageManager::install("freecad", family);
                    break;
                case 8:
                    PackageManager::install("opencad", family);
                    break;
                case 9:
                    PackageManager::install({"kicad-templates", "kicad"}, family);
                    break;
                case 10:
                    PackageManager::install("openscad", family);
                    break;
                case 11:
                    PackageManager::install({"gnuplot", "gnuplot-x11"}, family);
                    break;
                default: break;
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  R语言子菜单 (对应 Bash: beta_features:397-420)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_r_lang_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.r_title")
                               + "\" --menu \"" + _("beta.r_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.r_base") + "\" "
                               "\"2\" \"" + _("beta.rstudio") + "\" "
                               "\"3\" \"" + _("beta.r_recommended") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") {
                PackageManager::install("r-base", family);
            } else if (ch == "2") {
                // RStudio — 对应 Bash install_r_studio: amd64 only, distro-specific
                if (cfg_.arch != "amd64") {
                    Logger::error("RStudio requires amd64 architecture");
                    break;
                }
                if (cfg_.linux_distro == "arch") {
                    PackageManager::install("rstudio-desktop-git", family);
                } else if (cfg_.linux_distro == "debian") {
                    // 从 rstudio.com 抓取最新 .deb
                    Logger::step("Downloading latest RStudio...");
                    auto ver = Executor::shell(
                        "curl -sL 'https://rstudio.com/products/rstudio/download/#download' 2>/dev/null | "
                        "grep -oP 'rstudio-[\\d.]+-amd64\\.deb' | head -n1");
                    std::string deb = ver.ok() ? ver.stdout_data : "";
                    if (!deb.empty()) {
                        deb.erase(std::remove(deb.begin(), deb.end(), '\n'), deb.end());
                        Executor::passthrough("curl -L -o /tmp/" + deb + " "
                            "'https://download1.rstudio.org/electron/focal/amd64/" + deb + "' 2>/dev/null");
                        Executor::passthrough("apt install -y /tmp/" + deb + " 2>/dev/null");
                        Logger::ok("RStudio installed");
                    } else {
                        Logger::error("Could not detect latest RStudio version");
                    }
                } else if (cfg_.linux_distro == "redhat") {
                    auto ver = Executor::shell(
                        "curl -sL 'https://rstudio.com/products/rstudio/download/#download' 2>/dev/null | "
                        "grep -oP 'rstudio-[\\d.]+-x86_64\\.rpm' | head -n1");
                    std::string rpm = ver.ok() ? ver.stdout_data : "";
                    if (!rpm.empty()) {
                        rpm.erase(std::remove(rpm.begin(), rpm.end(), '\n'), rpm.end());
                        Executor::passthrough("curl -L -o /tmp/" + rpm + " "
                            "'https://download1.rstudio.org/electron/focal/amd64/" + rpm + "' 2>/dev/null");
                        Executor::passthrough("yum install -y /tmp/" + rpm + " 2>/dev/null");
                        Logger::ok("RStudio installed");
                    } else {
                        Logger::error("Could not detect latest RStudio version");
                    }
                } else {
                    Logger::warn("RStudio auto-download only supports Debian/Redhat/Arch");
                }
            } else if (ch == "3") {
                PackageManager::install("r-recommended", family);
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项7: 文件管理 (对应 Bash: beta_features:461-496)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_file_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.file_title")
                               + "\" --menu \"" + _("beta.file_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.file_fm") + "\" "
                               "\"2\" \"" + _("beta.file_catfish") + "\" "
                               "\"3\" \"" + _("beta.file_gparted") + "\" "
                               "\"4\" \"" + _("beta.file_baobab") + "\" "
                               "\"5\" \"" + _("beta.file_cfdisk") + "\" "
                               "\"6\" \"" + _("beta.file_partitionmgr") + "\" "
                               "\"7\" \"" + _("beta.file_mc") + "\" "
                               "\"8\" \"" + _("beta.file_ranger") + "\" "
                               "\"9\" \"" + _("beta.file_gnome_disks") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            static const std::string pkgs[] = {
                "__FILE_MANAGER__", // 1: 特殊处理 thunar/nautilus/dolphin
                "catfish", // 2
                "gparted", // 3
                "baobab", // 4
                "util-linux", // 5: cfdisk
                "partitionmanager", // 6
                "mc", // 7
                "ranger", // 8
                "gnome-disk-utility" // 9
            };
            int idx = std::stoi(ch) - 1;
            if (idx < 0 || idx >= 9) continue;

            if (idx == 0) {
                // 文件管理器选择器 (对应 Bash thunar_nautilus_dolphion)
                std::string fm_menu = cfg_.tui_bin +
                    " --title \"FILE MANAGER\" --menu \"Select file manager:\" 0 50 0 "
                    "\"1\" \"thunar (XFCE, lightweight)\" "
                    "\"2\" \"nautilus (GNOME Files)\" "
                    "\"3\" \"dolphin (KDE)\" "
                    "\"4\" \"thunar + nautilus\" "
                    "\"0\" \"" + _("menu.tui.back") + "\"";
                std::string fmpick = Executor::tui_select(fm_menu);
                if (fmpick == "1") PackageManager::install("thunar", family);
                else if (fmpick == "2") PackageManager::install("nautilus", family);
                else if (fmpick == "3") PackageManager::install("dolphin", family);
                else if (fmpick == "4") PackageManager::install({"thunar", "nautilus"}, family);
            } else {
                PackageManager::install(pkgs[idx], family);
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项8: 阅读器 (对应 Bash: beta_features:570-601)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_reader_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.reader_title")
                               + "\" --menu \"" + _("beta.reader_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.reader_calibre") + "\" "
                               "\"2\" \"" + _("beta.reader_fbreader") + "\" "
                               "\"3\" \"" + _("beta.reader_typora") + "\" "
                               "\"4\" \"" + _("beta.reader_xournal") + "\" "
                               "\"5\" \"" + _("beta.reader_evince") + "\" "
                               "\"6\" \"" + _("beta.reader_okular") + "\" "
                               "\"7\" \"" + _("beta.reader_kchmviewer") + "\" "
                               "\"8\" \"" + _("beta.reader_pdfchain") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            static const std::string pkgs[] = {
                "calibre", "fbreader", "typora",
                "xournal", "evince",
                "okular", // + okular-extra-backends
                "kchmviewer", "pdfchain"
            };
            int idx = std::stoi(ch) - 1;
            if (idx >= 0 && idx < 8) {
                if (idx == 5) {
                    // okular + extra backends
                    PackageManager::install({"okular", "okular-extra-backends"}, family);
                } else {
                    PackageManager::install(pkgs[idx], family);
                }
            }
            Logger::press_enter();
        }
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
        Logger::info("Network management sub-menu is not yet implemented.");
        Logger::info("It will cover: nmtui, WiFi scan, device management,");
        Logger::info("network card drivers (non-free firmware), IP display,");
        Logger::info("wifi-qr, manual config editing, blueman, gnome-nettool.");
        Logger::info("---");
        Logger::info("For now, use these commands directly:");
        Logger::info("  nmtui              — NetworkManager TUI");
        Logger::info("  nmcli device wifi   — WiFi scan & connect");
        Logger::info("  ip -br -c a        — view IP addresses");
    }

    // ═══════════════════════════════════════════════════════════════
    //  选项12: 其他 (对应 Bash: beta_features:50-78)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_other_menu() {
        while (true) {
            auto family = infer_family_from_config(cfg_.linux_distro);

            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.other_title")
                               + "\" --menu \"" + _("beta.other_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.other_obs") + "\" "
                               "\"2\" \"" + _("beta.other_seahorse") + "\" "
                               "\"3\" \"" + _("beta.other_kodi") + "\" "
                               "\"4\" \"" + _("beta.other_scrcpy") + "\" "
                               "\"5\" \"" + _("beta.other_flameshot") + "\" "
                               "\"6\" \"" + _("beta.other_telegram") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            switch (std::stoi(ch)) {
                case 1:
                    // OBS Studio — Gentoo特殊包名
                {
                    std::string obs_pkg = (cfg_.linux_distro == "gentoo")
                                              ? "media-video/obs-studio"
                                              : "obs-studio";
                    PackageManager::install(obs_pkg, family);
                }
                break;
                case 2: PackageManager::install("seahorse", family);
                    break;
                case 3:
                    PackageManager::install({"kodi", "kodi-wayland"}, family);
                    break;
                case 4: run_scrcpy_menu();
                    break;
                case 5: PackageManager::install("flameshot", family);
                    break;
                case 6: PackageManager::install("telegram-desktop", family);
                    break;
                default: break;
            }
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  scrcpy 子菜单 (对应 Bash: beta_features:92-118)
    // ═══════════════════════════════════════════════════════════════

    void BetaFeaturesManager::run_scrcpy_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"scrcpy\""
                               + " --menu \"" + _("beta.scrcpy_prompt") + "\" 0 50 0 "
                               "\"1\" \"" + _("beta.scrcpy_install") + "\" "
                               "\"2\" \"" + _("beta.scrcpy_connect") + "\" "
                               "\"3\" \"" + _("beta.scrcpy_switch") + "\" "
                               "\"4\" \"" + _("beta.scrcpy_restart_adb") + "\" "
                               "\"5\" \"" + _("beta.scrcpy_readme") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            switch (std::stoi(ch)) {
                case 1: {
                    auto family = infer_family_from_config(cfg_.linux_distro);
                    PackageManager::install("scrcpy", family);
                    break;
                }
                // ── adb 连接管理 (对应 Bash scrcpy_connect_to_android_device) ──
                case 2: {
                    std::string target = Executor::tui_select(
                        cfg_.tui_bin + " --title \"ADB ADDRESS\""
                        " --inputbox \"Enter adb address (e.g. 192.168.99.3:5555)\" 0 0");
                    if (target.empty()) target = "localhost:5555";
                    if (target.find(':') == std::string::npos) target += ":5555";
                    Executor::passthrough("adb connect " + target + " 2>/dev/null");
                    Executor::passthrough("adb devices -l 2>/dev/null");
                    Logger::info("You can run scrcpy now. Start it?");
                    break;
                }
                // ── 切换 scrcpy 设备 (对应 Bash switch_scrcpy_device) ──
                case 3: {
                    auto devs = Executor::shell("adb devices 2>/dev/null | sed '1d;$d' | awk '{print $1}'");
                    std::string dlist = devs.ok() ? devs.stdout_data : "";
                    if (dlist.empty()) { Logger::warn("no adb devices found"); break; }
                    std::string dmenu = cfg_.tui_bin + " --title \"SCRCPY DEVICES\" --menu \"Switch to:\" 0 0 0 ";
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
                    if (dpick == "0" || dpick.empty()) break;
                    int didx = std::stoi(dpick) - 1;
                    if (didx >= 0 && didx < (int)devices.size())
                        Executor::passthrough("scrcpy -s " + devices[didx] + " 2>/dev/null &");
                    break;
                }
                case 4:
                    Executor::passthrough("adb kill-server 2>/dev/null || true");
                    Executor::passthrough("adb devices -l 2>/dev/null || true");
                    break;
                // ── scrcpy FAQ (对应 Bash scrpy_faq) ──
                case 5:
                    Logger::info("scrcpy FAQ:");
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
                    break;
                default: break;
            }
            Logger::press_enter();
        }
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
