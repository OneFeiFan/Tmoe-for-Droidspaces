#include "beta_features.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

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
            Logger::press_enter();
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
                case 1: placeholder_return("sudo user group management");
                    break;
                case 2: placeholder_return("rc.local systemd script");
                    break;
                case 3: placeholder_return("UEFI boot manager");
                    break;
                case 4: PackageManager::install("gnome-system-monitor", family);
                    break;
                case 5: PackageManager::install("grub-customizer", family);
                    break;
                case 6:
                    PackageManager::install({"gnome-system-tools", "gnome-logs"}, family);
                    break;
                case 7: placeholder_return("boot-repair (ppa:yannubuntu)");
                    break;
                case 8: // neofetch
                    placeholder("neofetch (will install & run)");
                    break;
                case 9: // yasat
                    placeholder("yasat --full-scan");
                    break;
                case 10: placeholder_return("Tmoe-linux manager (old version)");
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
                        placeholder("aptitude (Debian only)");
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
                // RStudio — 架构/发行版判断 (对应 Bash install_r_studio)
                placeholder("RStudio (arch/distro-specific download)");
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
                // 文件管理器三选一 (对应 Bash thunar_nautilus_dolphion)
                placeholder_return("file manager: thunar / nautilus / dolphin (interactive pick)");
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

    void BetaFeaturesManager::run_network_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.net_title")
                               + "\" --menu \"" + _("beta.net_prompt") + "\" 17 50 8 "
                               "\"1\" \"" + _("beta.net_nmtui") + "\" "
                               "\"2\" \"" + _("beta.net_enable_dev") + "\" "
                               "\"3\" \"" + _("beta.net_wifi_scan") + "\" "
                               "\"4\" \"" + _("beta.net_dev_status") + "\" "
                               "\"5\" \"" + _("beta.net_driver") + "\" "
                               "\"6\" \"" + _("beta.net_view_ip") + "\" "
                               "\"7\" \"" + _("beta.net_wifi_qr") + "\" "
                               "\"8\" \"" + _("beta.net_edit_config") + "\" "
                               "\"9\" \"" + _("beta.net_autostart") + "\" "
                               "\"10\" \"" + _("beta.net_blueman") + "\" "
                               "\"11\" \"" + _("beta.net_gnome_nettool") + "\" "
                               "\"0\" \"" + _("menu.tui.back") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            auto family = infer_family_from_config(cfg_.linux_distro);

            switch (std::stoi(ch)) {
                case 1: placeholder_return("nmtui (NetworkManager TUI)");
                    break;
                case 2: placeholder_return("enable network device");
                    break;
                case 3: placeholder_return("WiFi scan (iw/iwlist)");
                    break;
                case 4: placeholder_return("network device status");
                    break;
                case 5: placeholder_return("network card driver install");
                    break;
                case 6: placeholder_return("view IP address");
                    break;
                case 7: placeholder_return("WiFi QR code");
                    break;
                case 8: placeholder_return("edit network config");
                    break;
                case 9: placeholder_return("systemctl enable NetworkManager");
                    break;
                case 10:
                    // blueman
                    if (cfg_.linux_distro == "alpine")
                        PackageManager::install({"gnome-bluetooth", "blueman"}, family);
                    else
                        PackageManager::install({"blueman-manager", "blueman"}, family);
                    break;
                case 11:
                    // gnome-nettool
                    if (cfg_.linux_distro == "debian")
                        PackageManager::install({"gnome-nettool", "network-manager-gnome"}, family);
                    else
                        PackageManager::install({"gnome-nettool", "gnome-network-manager"}, family);
                    break;
                default: break;
            }
            Logger::press_enter();
        }
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
                case 2: placeholder_return("adb connect (interactive address input)");
                    break;
                case 3: placeholder_return("switch scrcpy device (adb devices list)");
                    break;
                case 4:
                    Executor::passthrough("adb kill-server 2>/dev/null || true");
                    Executor::passthrough("adb devices -l 2>/dev/null || true");
                    break;
                case 5: placeholder_return("scrcpy FAQ / readme");
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

    void BetaFeaturesManager::placeholder(const std::string &feature_name) {
        Logger::warn(_("misc.not_implemented") + ": " + feature_name);
    }

    void BetaFeaturesManager::placeholder_return(const std::string &feature_name) {
        Logger::warn(_("misc.not_implemented") + ": " + feature_name);
        Logger::info(_("misc.press_enter"));
    }
} // namespace tmoe::domain
