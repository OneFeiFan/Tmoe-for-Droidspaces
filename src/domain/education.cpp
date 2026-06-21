#include "education.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

namespace tmoe::domain {
    EducationManager::EducationManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void EducationManager::run_education_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("edu.menu_title")
                               + "\" --menu \"" + _("edu.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("edu.math") + "\" "
                               "\"2\" \"" + _("edu.chemistry") + "\" "
                               "\"3\" \"" + _("edu.physics") + "\" "
                               "\"4\" \"" + _("edu.english") + "\" "
                               "\"5\" \"" + _("edu.exam") + "\" "
                               "\"6\" \"" + _("edu.notes") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;
            if (ch == "1") run_math_menu();
            else if (ch == "2") run_chemistry_menu();
            else if (ch == "3") run_physics_menu();
            else if (ch == "4") run_english_menu();
            else if (ch == "5") run_exam_menu();
            else if (ch == "6") {
                auto family = infer_family_from_config(cfg_.linux_distro);
                PackageManager::install("xournal", family);
            }
            Logger::press_enter();
        }
    }

    void EducationManager::run_math_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.math_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.geogebra") + "\" "
                           "\"2\" \"" + _("edu.octave") + "\" "
                           "\"3\" \"" + _("edu.scilab") + "\" "
                           "\"4\" \"" + _("edu.freemat") + "\" "
                           "\"5\" \"" + _("edu.maxima") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        if (ch == "1") PackageManager::install("geogebra", family);
        else if (ch == "2") PackageManager::install("octave", family);
        else if (ch == "3") PackageManager::install("scilab", family);
        else if (ch == "4") PackageManager::install("freemat", family);
        else if (ch == "5") PackageManager::install({"maxima", "wxmaxima"}, family);
        Logger::press_enter();
    }

    void EducationManager::run_chemistry_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.chem_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.kalzium") + "\" "
                           "\"2\" \"" + _("edu.nwchem") + "\" "
                           "\"3\" \"" + _("edu.avogadro") + "\" "
                           "\"4\" \"" + _("edu.pymol") + "\" "
                           "\"5\" \"" + _("edu.gromacs") + "\" "
                           "\"6\" \"" + _("edu.cp2k") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        if (ch == "1") PackageManager::install("kalzium", family);
        else if (ch == "2") PackageManager::install("nwchem", family);
        else if (ch == "3") PackageManager::install("avogadro", family);
        else if (ch == "4") PackageManager::install("pymol", family);
        else if (ch == "5") PackageManager::install("gromacs", family);
        else if (ch == "6") PackageManager::install("cp2k", family);
        Logger::press_enter();
    }

    void EducationManager::run_physics_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.physics_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.step") + "\" "
                           "\"2\" \"" + _("edu.openfoam") + "\" "
                           "\"3\" \"" + _("edu.geant4") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        if (ch == "1") PackageManager::install("step", family);
        else if (ch == "2") PackageManager::install("openfoam", family);
        else if (ch == "3") PackageManager::install("geant321", family);
        Logger::press_enter();
    }

    void EducationManager::run_english_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.english_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.goldendict") + "\" "
                           "\"2\" \"" + _("edu.goldendict_wordnet") + "\" "
                           "\"3\" \"" + _("edu.fcitx5_zhwiki") + "\" "
                           "\"4\" \"" + _("edu.fcitx5_moegirl") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        if (ch == "1") PackageManager::install("goldendict", family);
        else if (ch == "2") PackageManager::install({"goldendict", "goldendict-wordnet"}, family);
        else if (ch == "3") PackageManager::install("fcitx5-pinyin-zhwiki", family);
        else if (ch == "4") PackageManager::install("fcitx5-pinyin-moegirl", family);
        Logger::press_enter();
    }

    void EducationManager::run_exam_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("edu.exam_menu")
                               + "\" --menu \"\" 0 0 0 "
                               "\"1\" \"" + _("edu.gaokao") + "\" "
                               "\"2\" \"" + _("edu.cet4") + "\" "
                               "\"3\" \"" + _("edu.cet6") + "\" "
                               "\"4\" \"" + _("edu.kaoyan") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;
            if (ch == "1") run_gaokao_menu();
            else if (ch == "2") download_exam_data("CET-4", "cet4");
            else if (ch == "3") download_exam_data("CET-6", "cet6");
            else if (ch == "4") run_kaoyan_menu();
            Logger::press_enter();
        }
    }

    void EducationManager::run_gaokao_menu() {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.gaokao_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.gaokao_math") + "\" "
                           "\"2\" \"" + _("edu.gaokao_physics") + "\" "
                           "\"3\" \"" + _("edu.gaokao_chemistry") + "\" "
                           "\"4\" \"" + _("edu.gaokao_biology") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        std::string subject = (ch == "1")
                                  ? "math"
                                  : (ch == "2")
                                        ? "physics"
                                        : (ch == "3")
                                              ? "chemistry"
                                              : "biology";
        download_exam_data(_("edu.gaokao"), "gaokao_" + subject);
        Logger::press_enter();
    }

    void EducationManager::run_kaoyan_menu() {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.kaoyan_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("edu.kaoyan_politics") + "\" "
                           "\"2\" \"" + _("edu.kaoyan_english") + "\" "
                           "\"3\" \"" + _("edu.kaoyan_math") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;
        std::string subject = (ch == "1") ? "politics" : (ch == "2") ? "english" : "math";
        download_exam_data(_("edu.kaoyan"), "kaoyan_" + subject);
        Logger::press_enter();
    }

    void EducationManager::download_exam_data(const std::string &desc,
                                              const std::string &folder_name) {
        Logger::info(_("edu.downloading") + ": " + desc);
        std::string dest = "/tmp/tmoe_study/" + folder_name;
        Executor::shell("mkdir -p " + dest);
        // 从 Gitee 仓库 clone + tar 解压
        std::string cmd = "cd " + dest + " && "
                          "if [ ! -d .git ]; then "
                          "  git clone --depth=1 https://gitee.com/mo2/linux-study.git . 2>/dev/null || "
                          "  (git clone --depth=1 https://github.com/2moe/tmoe-study.git . 2>/dev/null); "
                          "fi";
        Executor::shell(cmd);
        Logger::ok(_("edu.download_done"));
    }
} // namespace tmoe::domain
