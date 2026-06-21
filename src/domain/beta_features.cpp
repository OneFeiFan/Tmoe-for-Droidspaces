#include "beta_features.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

namespace tmoe::domain {
    BetaFeaturesManager::BetaFeaturesManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void BetaFeaturesManager::run_beta_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("beta.menu_title")
                               + "\" --menu \"" + _("beta.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("beta.flameshot") + "\" "
                               "\"2\" \"" + _("beta.telegram") + "\" "
                               "\"3\" \"" + _("beta.scrcpy") + "\" "
                               "\"4\" \"" + _("beta.seahorse") + "\" "
                               "\"5\" \"" + _("beta.kodi") + "\" "
                               "\"6\" \"" + _("beta.aptitude") + "\" "
                               "\"7\" \"" + _("beta.deepin_store") + "\" "
                               "\"8\" \"" + _("beta.gnome_store") + "\" "
                               "\"9\" \"" + _("beta.flatpak_snap") + "\" "
                               "\"10\" \"" + _("beta.drawing") + "\" "
                               "\"11\" \"" + _("beta.r_lang") + "\" "
                               "\"12\" \"" + _("beta.file_mgr") + "\" "
                               "\"13\" \"" + _("beta.multimedia") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") install_single("beta.flameshot", "flameshot");
            else if (ch == "2") install_single("beta.telegram", "telegram-desktop");
            else if (ch == "3") install_single("beta.scrcpy", "scrcpy");
            else if (ch == "4") install_single("beta.seahorse", "seahorse");
            else if (ch == "5") install_single("beta.kodi", "kodi");
            else if (ch == "6") install_single("beta.aptitude", "aptitude");
            else if (ch == "7") run_deepin_menu();
            else if (ch == "8") run_store_menu();
            else if (ch == "9") {
                auto family = infer_family_from_config(cfg_.linux_distro);
                PackageManager::install({
                                            "flatpak", "gnome-software-plugin-flatpak",
                                            "plasma-discover-backend-flatpak", "snapd"
                                        }, family);
            } else if (ch == "10") run_drawing_menu();
            else if (ch == "11") run_rlang_menu();
            else if (ch == "12") run_file_menu();
            else if (ch == "13") run_multimedia_menu();
            Logger::press_enter();
        }
    }

    void BetaFeaturesManager::run_store_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.store_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.gnome_store") + "\" "
                           "\"2\" \"" + _("beta.flatpak_snap") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install({"gnome-software", "plasma-discover"}, family);
        else if (ch == "2")
            PackageManager::install({
                                        "flatpak", "gnome-software-plugin-flatpak",
                                        "plasma-discover-backend-flatpak", "snapd",
                                        "qbittorrent"
                                    }, family);
    }

    void BetaFeaturesManager::run_deepin_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.deepin_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.deepin_calculator") + "\" "
                           "\"2\" \"" + _("beta.deepin_screen_recorder") + "\" "
                           "\"3\" \"" + _("beta.deepin_album") + "\" "
                           "\"4\" \"" + _("beta.deepin_draw") + "\" "
                           "\"5\" \"" + _("beta.deepin_music") + "\" "
                           "\"6\" \"" + _("beta.deepin_movie") + "\" "
                           "\"7\" \"" + _("beta.deepin_compressor") + "\" "
                           "\"8\" \"" + _("beta.deepin_picker") + "\" "
                           "\"9\" \"" + _("beta.deepin_font_manager") + "\" "
                           "\"10\" \"" + _("beta.deepin_system_monitor") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        std::vector<std::string> deepin_list = {
            "deepin-calculator", "deepin-screen-recorder", "deepin-album",
            "deepin-draw", "deepin-music", "deepin-movie", "deepin-compressor",
            "deepin-picker", "deepin-font-manager", "deepin-system-monitor"
        };
        int idx = std::stoi(ch) - 1;
        if (idx >= 0 && idx < static_cast<int>(deepin_list.size()))
            PackageManager::install(deepin_list[idx], family);
    }

    void BetaFeaturesManager::run_drawing_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.drawing_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.krita") + "\" "
                           "\"2\" \"" + _("beta.inkscape") + "\" "
                           "\"3\" \"" + _("beta.kolourpaint") + "\" "
                           "\"4\" \"" + _("beta.latexdraw") + "\" "
                           "\"5\" \"" + _("beta.librecad") + "\" "
                           "\"6\" \"" + _("beta.freecad") + "\" "
                           "\"7\" \"" + _("beta.kicad") + "\" "
                           "\"8\" \"" + _("beta.openscad") + "\" "
                           "\"9\" \"" + _("beta.gnuplot") + "\" "
                           "\"10\" \"" + _("beta.rstudio") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install("krita", family);
        else if (ch == "2") PackageManager::install("inkscape", family);
        else if (ch == "3") PackageManager::install("kolourpaint", family);
        else if (ch == "4") PackageManager::install("latexdraw", family);
        else if (ch == "5") PackageManager::install("librecad", family);
        else if (ch == "6") PackageManager::install("freecad", family);
        else if (ch == "7") PackageManager::install("kicad", family);
        else if (ch == "8") PackageManager::install("openscad", family);
        else if (ch == "9") PackageManager::install("gnuplot", family);
        else if (ch == "10") PackageManager::install("rstudio-desktop-git", family);
    }

    void BetaFeaturesManager::run_rlang_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.r_lang_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.r_base") + "\" "
                           "\"2\" \"" + _("beta.rstudio") + "\" "
                           "\"3\" \"" + _("beta.r_rstudio") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        if (ch == "1") PackageManager::install("r-base", family);
        else if (ch == "2") PackageManager::install("rstudio-desktop-git", family);
        else if (ch == "3") PackageManager::install({"r-base", "rstudio-desktop-git"}, family);
    }

    void BetaFeaturesManager::run_file_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.file_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.thunar") + "\" "
                           "\"2\" \"" + _("beta.nautilus") + "\" "
                           "\"3\" \"" + _("beta.dolphin") + "\" "
                           "\"4\" \"" + _("beta.catfish") + "\" "
                           "\"5\" \"" + _("beta.gparted") + "\" "
                           "\"6\" \"" + _("beta.baobab") + "\" "
                           "\"7\" \"" + _("beta.gnome_disk") + "\" "
                           "\"8\" \"" + _("beta.partitionmanager") + "\" "
                           "\"9\" \"" + _("beta.mc") + "\" "
                           "\"10\" \"" + _("beta.ranger") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install("thunar", family);
        else if (ch == "2") PackageManager::install("nautilus", family);
        else if (ch == "3") PackageManager::install("dolphin", family);
        else if (ch == "4") PackageManager::install("catfish", family);
        else if (ch == "5") PackageManager::install("gparted", family);
        else if (ch == "6") PackageManager::install("baobab", family);
        else if (ch == "7") PackageManager::install("gnome-disk-utility", family);
        else if (ch == "8") PackageManager::install("partitionmanager", family);
        else if (ch == "9") PackageManager::install("mc", family);
        else if (ch == "10") PackageManager::install("ranger", family);
    }

    void BetaFeaturesManager::run_multimedia_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("beta.multimedia_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("beta.obs_studio") + "\" "
                           "\"2\" \"" + _("beta.evince") + "\" "
                           "\"3\" \"" + _("beta.okular") + "\" "
                           "\"4\" \"" + _("beta.kchmviewer") + "\" "
                           "\"5\" \"" + _("beta.pdfchain") + "\" "
                           "\"6\" \"" + _("beta.xournal") + "\" "
                           "\"7\" \"" + _("beta.calibre") + "\" "
                           "\"8\" \"" + _("beta.fbreader") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install("obs-studio", family);
        else if (ch == "2") PackageManager::install("evince", family);
        else if (ch == "3") PackageManager::install("okular", family);
        else if (ch == "4") PackageManager::install("kchmviewer", family);
        else if (ch == "5") PackageManager::install("pdfchain", family);
        else if (ch == "6") PackageManager::install("xournal", family);
        else if (ch == "7") PackageManager::install("calibre", family);
        else if (ch == "8") PackageManager::install("fbreader", family);
    }

    void BetaFeaturesManager::install_single(const std::string &i18n_key,
                                             const std::string &pkg) {
        Logger::step(_(i18n_key));
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install(pkg, family);
    }

    void BetaFeaturesManager::install_multi(const std::vector<std::string> &keys,
                                            const std::vector<std::string> &pkgs) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install(pkgs, family);
    }
} // namespace tmoe::domain
