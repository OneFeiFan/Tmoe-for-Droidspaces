#include "domain/features/education.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/command_builder.hpp"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

EducationManager::EducationManager(const TmoeConfig &cfg) : cfg_(cfg) {
    download_folder_ = get_download_folder();
}

// ═══════════════════════════════════════════════════════════════
//  辅助
// ═══════════════════════════════════════════════════════════════

std::string EducationManager::get_download_folder() const {
    // 对应 Bash check_tmoe_study_materials: WSL vs 普通环境
    if (cfg_.is_wsl) {
        return "/mnt/c/Users/Public/Documents";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/sd/Download/Documents";
    }
    return "/tmp/tmoe_study";
}

bool EducationManager::whiptail_yesno(const std::string& title,
                                       const std::string& yes_btn,
                                       const std::string& no_btn,
                                       const std::string& prompt,
                                       int height, int width) {
    std::string cmd = cfg_.tui_bin +
        " --title \"" + title + "\"" +
        " --yes-button \"" + yes_btn + "\"" +
        " --no-button \"" + no_btn + "\"" +
        " --yesno \"" + prompt + "\" " +
        std::to_string(height) + " " + std::to_string(width);
    auto r = Executor::passthrough(cmd);
    return r.exit_code == 0;
}

// ═══════════════════════════════════════════════════════════════
//  核心下载模型 (对应 Bash check_tmoe_study_materials 134-187行)
// ═══════════════════════════════════════════════════════════════

void EducationManager::download_study_materials(const std::string& repo_url,
                                                 const std::string& branch,
                                                 const std::string& filename) {
    // 对应 Bash: mkdir -pv ${DOWNLOAD_FOLDER} && cd ${DOWNLOAD_FOLDER}
    CommandBuilder("mkdir").add_flag("-p").add_arg(download_folder_).execute();

    std::string file_path = download_folder_ + "/" + filename;

    // 对应 Bash: if [ -e ${DOWNLOAD_FILE_NAME} ]
    if (fs::exists(file_path)) {
        Logger::info(_("edu.file_detected") + ": " + filename);
        // 对应 Bash: whiptail --yes-button '解压uncompress' --no-button '重下DL again'
        if (whiptail_yesno(
                _("edu.file_detected_title"),
                _("edu.btn_uncompress"),
                _("edu.btn_redownload"),
                _("edu.file_detected_prompt") + "\n" + filename)) {
            // 解压
            Logger::step(_("edu.uncompressing") + ": " + filename);
            Executor::passthrough("cd \"" + download_folder_ + "\" && "
                                  "tar -Jxvf \"" + filename + "\" 2>&1");
            Logger::ok(_("edu.uncompress_done") + ": " + download_folder_);
            if (cfg_.is_wsl) {
                Logger::info(_("edu.wsl_hint"));
            }
            return;
        }
        // else: 重下 → 继续到 git clone
    }

    // 对应 Bash: git_clone_tmoe_study_file (164-187行)
    Logger::step(_("edu.downloading") + ": " + filename);
    std::string temp_dir = "/tmp/.TMOE_STUDY_MATERIALS_TEMP_FOLDER";
    std::string inner_temp = "/tmp/.TMOE_STUDY_MATERIALS_TEMP_FOLDER/."
                             + filename + "_TEMP_FOLDER_01";

    CommandBuilder("rm").add_flag("-rf").add_arg(temp_dir).add_raw("2>/dev/null").execute();
    CommandBuilder("mkdir").add_flag("-p").add_arg(temp_dir).execute();

    // git clone --depth=1 -b ${BRANCH_NAME} ${REPO_URL} ${TEMP_FOLDER}
    int ret = Executor::passthrough(
        "cd \"" + temp_dir + "\" && "
        "git clone --depth=1 -b " + branch + " " + repo_url + " \"." + filename + "_TEMP_FOLDER_01\" 2>&1"
    ).exit_code;

    if (ret != 0) {
        Logger::error(_("edu.download_failed"));
        CommandBuilder("rm").add_flag("-rf").add_arg(temp_dir).add_raw("2>/dev/null").execute();
        return;
    }

    // 对应 Bash: cd ${TMOE_TEMP_FOLDER} && mv .study_* .. && cd .. && cat .study_* >${DOWNLOAD_FILE_NAME}
    Executor::shell(
        "cd \"" + inner_temp + "\" && "
        "for f in .study_*; do [ -f \"$f\" ] && mv \"$f\" .. 2>/dev/null; done; "
        "cd \"" + temp_dir + "\" && "
        "cat .study_* > \"" + file_path + "\" 2>/dev/null; "
        "rm -rf \"" + temp_dir + "\" 2>/dev/null"
    );

    // 解压
    Logger::step(_("edu.uncompressing") + ": " + filename);
    Executor::passthrough("cd \"" + download_folder_ + "\" && "
                          "tar -Jxvf \"" + filename + "\" 2>&1");

    Logger::ok(_("edu.download_done") + ": " + download_folder_);
    if (cfg_.is_wsl) {
        Logger::info(_("edu.wsl_hint"));
    }
}

// ═══════════════════════════════════════════════════════════════
//  L1: 科学与教育主菜单 (对应 Bash tmoe_education_app_menu 362-389行)
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_education_menu() {
    while (true) {
        // 对应 Bash: 6个子选项 + 返回
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.menu_title")
                         + "\" --menu \"" + _("edu.menu_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.gaokao") + "\" "            // 高考
                         "\"2\" \"" + _("edu.kaoyan") + "\" "            // 考研
                         "\"3\" \"" + _("edu.math") + "\" "              // 数学
                         "\"4\" \"" + _("edu.english") + "\" "           // 英语
                         "\"5\" \"" + _("edu.physics") + "\" "           // 物理
                         "\"6\" \"" + _("edu.chemistry") + "\" "         // 化学
                         "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") run_gaokao_menu();
        else if (ch == "2") run_kaoyan_menu();
        else if (ch == "3") run_math_menu();
        else if (ch == "4") run_english_menu();
        else if (ch == "5") run_physics_menu();
        else if (ch == "6") run_chemistry_menu();

        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 高考 (对应 Bash tmoe_college_entrance_examination 309-326行)
//    → L3: 真题 / 学习笔记
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_gaokao_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.gaokao_title")
                         + "\" --menu \"" + _("edu.gaokao_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.gaokao_papers") + "\" "      // 真题
                         "\"2\" \"" + _("edu.gaokao_notes") + "\" "       // 学习笔记
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") run_gaokao_papers_menu();
        else if (ch == "2") run_gaokao_notes_menu();

        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L3: 高考真题 (对应 Bash college_entrance_examination_paper 241-279行)
//    4个年份版本，从 gitee.com/ak2 下载
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_gaokao_papers_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.gaokao_papers_title")
                         + "\" --menu \"" + _("edu.gaokao_papers_prompt") + "\" 0 50 0 "
                         "\"1\" \"2020 (" + _("edu.size_79mb") + ")\" "
                         "\"2\" \"2008-2019 (" + _("edu.no_listening") + ", 392.2MiB)\" "
                         "\"3\" \"2013-2018 (" + _("edu.science_edition") + ", 146.3MiB)\" "
                         "\"4\" \"2008-2018 (" + _("edu.english_listening_only") + ", 244.9MiB)\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        switch (std::stoi(ch)) {
        case 1:
            download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2020",
                "2020", "2020年高考真题.tar.xz");
            break;
        case 2:
            download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2019",
                "2019", "2008-2019高考真题.tar.xz");
            break;
        case 3:
            download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2013_to_2018",
                "2018", "2013-2018高考真题.tar.xz");
            break;
        case 4:
            download_study_materials(
                "https://gitee.com/ak2/gaokao_english_listening",
                "2018", "2008-2018高考英语听力.tar.xz");
            break;
        default: break;
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L3: 高考学习笔记 (对应 Bash college_entrance_examination_notes 281-307行)
//    生物 / 英语
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_gaokao_notes_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.gaokao_notes_title")
                         + "\" --menu \"" + _("edu.gaokao_notes_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.gaokao_bio_notes") + " (131.8MiB)\" "
                         "\"2\" \"" + _("edu.gaokao_eng_notes") + " (5.4MiB)\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            download_study_materials(
                "https://gitee.com/ak2/biology_note",
                "2019", "生物笔记.tar.xz");
        } else if (ch == "2") {
            download_study_materials(
                "https://gitee.com/ak2/english_note",
                "2020", "英语终极笔记.tar.xz");
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 考研 (对应 Bash tmoe_postgraduate_entrance_examination 328-360行)
//    政治 / 英语 / 数学 真题
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_kaoyan_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.kaoyan_title")
                         + "\" --menu \"" + _("edu.kaoyan_prompt") + "\" 0 50 0 "
                         "\"1\" \"2003-2019 " + _("edu.kaoyan_politics") + " (6.2MiB)\" "
                         "\"2\" \"2001-2019 " + _("edu.kaoyan_english") + " (7.7MiB)\" "
                         "\"3\" \"1987-2020 " + _("edu.kaoyan_math") + " (" + _("edu.with_solutions") + ", 15.5MiB)\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            download_study_materials(
                "https://gitee.com/ak2/postgraduate_politics",
                "2019", "2003-2019政治真题.tar.xz");
        } else if (ch == "2") {
            download_study_materials(
                "https://gitee.com/ak2/postgraduate_english",
                "2019", "2001-2019英语真题.tar.xz");
        } else if (ch == "3") {
            download_study_materials(
                "https://gitee.com/ak2/postgraduate_math",
                "2020", "1987-2020数学真题.tar.xz");
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 数学 (对应 Bash tmoe_mathematics_menu 1-36行)
//    geogebra / octave / scilab / freemat / maxima
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_math_menu() {
    while (true) {
        auto family = infer_family_from_config(cfg_.linux_distro);

        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.math_title")
                         + "\" --menu \"" + _("edu.math_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.geogebra") + "\" "
                         "\"2\" \"" + _("edu.octave") + "\" "
                         "\"3\" \"" + _("edu.scilab") + "\" "
                         "\"4\" \"" + _("edu.freemat") + "\" "
                         "\"5\" \"" + _("edu.maxima") + "\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        switch (std::stoi(ch)) {
        case 1: PackageManager::install("geogebra", family); break;
        case 2: PackageManager::install("octave", family); break;
        case 3:
            // 对应 Bash: DEPENDENCY_01='scilab-minimal-bin' + DEPENDENCY_02='scilab'
            PackageManager::install({"scilab-minimal-bin", "scilab"}, family);
            break;
        case 4:
            PackageManager::install({"freemat", "freemat-help"}, family);
            break;
        case 5:
            PackageManager::install({"maxima", "wxmaxima"}, family);
            break;
        default: break;
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 英语 (对应 Bash tmoe_english_menu 220-239行)
//    goldendict → L3 / 四六级 → L3 / 名著 (直接下载)
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_english_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.english_title")
                         + "\" --menu \"" + _("edu.english_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.goldendict") + "\" "               // → L3
                         "\"2\" \"" + _("edu.cet_title") + "\" "                // → L3
                         "\"3\" \"" + _("edu.english_masterpieces") + " (222.8MiB)\" "  // 直接下载
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            run_goldendict_menu();
        } else if (ch == "2") {
            run_cet_menu();
        } else if (ch == "3") {
            // 对应 Bash: download_english_masterpieces
            download_study_materials(
                "https://gitee.com/ak2/masterpieces",
                "2018", "英文原著.tar.xz");
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L3: GoldenDict (对应 Bash tmoe_golden_dict_menu 115-132行)
//    安装 / 词库
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_goldendict_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.goldendict_title")
                         + "\" --menu \"" + _("edu.goldendict_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.goldendict_install") + "\" "
                         "\"2\" \"" + _("edu.goldendict_dicts") + "\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            auto family = infer_family_from_config(cfg_.linux_distro);
            PackageManager::install({"goldendict", "goldendict-wordnet"}, family);
        } else if (ch == "2") {
            // 对应 Bash tips_of_dict: 因版权问题不开放
            Logger::warn(_("edu.dict_copyright_notice"));
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L3: 四六级真题 (对应 Bash cet4_and_6_exam_paper 203-217行)
//    2013-2019 CET (6.7MiB)
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_cet_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.cet_title")
                         + "\" --menu \"" + _("edu.cet_prompt") + "\" 0 50 0 "
                         "\"1\" \"2013-2019 (" + _("edu.cet_both") + ", 6.7MiB)\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            // 对应 Bash: download_2013_to_2019_cet4_and_6_exam_paper
            download_study_materials(
                "https://gitee.com/ak2/cet",
                "2019", "cet.tar.xz");
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 物理 (对应 Bash tmoe_physics_menu 70-100行)
//    Step / OpenFOAM / Geant4
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_physics_menu() {
    while (true) {
        auto family = infer_family_from_config(cfg_.linux_distro);

        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.physics_title")
                         + "\" --menu \"" + _("edu.physics_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.step") + "\" "
                         "\"2\" \"" + _("edu.openfoam") + "\" "
                         "\"3\" \"" + _("edu.geant4") + "\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        switch (std::stoi(ch)) {
        case 1:
            // 对应 Bash: 先输出说明，再安装 step
            Logger::info(_("edu.step_desc"));
            PackageManager::install("step", family);
            break;
        case 2: PackageManager::install("openfoam", family); break;
        case 3: PackageManager::install("geant321", family); break;
        default: break;
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  L2: 化学 (对应 Bash tmoe_chemistry_menu 38-68行)
//    kalzium / nwchem / avogadro / pymol / Psi4 / gromacs / CP2K
// ═══════════════════════════════════════════════════════════════

void EducationManager::run_chemistry_menu() {
    while (true) {
        auto family = infer_family_from_config(cfg_.linux_distro);

        // 对应 Bash: 7个子选项 (比当前C++版多了 Psi4)
        std::string menu = cfg_.tui_bin + " --title \"" + _("edu.chemistry_title")
                         + "\" --menu \"" + _("edu.chemistry_prompt") + "\" 0 50 0 "
                         "\"1\" \"" + _("edu.kalzium") + "\" "
                         "\"2\" \"" + _("edu.nwchem") + "\" "
                         "\"3\" \"" + _("edu.avogadro") + "\" "
                         "\"4\" \"" + _("edu.pymol") + "\" "
                         "\"5\" \"" + _("edu.psi4") + "\" "
                         "\"6\" \"" + _("edu.gromacs") + "\" "
                         "\"7\" \"" + _("edu.cp2k") + "\" "
                         "\"0\" \"" + _("menu.tui.back") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        switch (std::stoi(ch)) {
        case 1: PackageManager::install("kalzium", family); break;
        case 2: PackageManager::install("nwchem", family); break;
        case 3: PackageManager::install("avogadro", family); break;
        case 4: PackageManager::install("pymol", family); break;
        case 5: PackageManager::install("psi4", family); break;
        case 6: PackageManager::install("gromacs", family); break;
        case 7: PackageManager::install("cp2k", family); break;
        default: break;
        }
        Logger::press_enter();
    }
}

} // namespace tmoe::domain
