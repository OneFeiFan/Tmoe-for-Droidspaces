/** Education 菜单插件 — 将 EducationManager 的 whiptail 子菜单迁移至 IUIMenu 框架。
 *  菜单层级：
 *    L1 科学与教育 (6项)
 *      ├─ 高考 → L2 gaokao_menu (2项容器)
 *      │   ├─ 真题 → L3 gaokao_papers_menu (4项)
 *      │   └─ 学习笔记 → L3 gaokao_notes_menu (2项)
 *      ├─ 考研 → L2 kaoyan_menu (3项)
 *      ├─ 数学 → L2 math_menu (5项)
 *      ├─ 英语 → L2 english_menu (3项容器)
 *      │   ├─ GoldenDict → L3 goldendict_menu (2项)
 *      │   ├─ 四六级 → L3 cet_menu (1项)
 *      │   └─ 名著下载 (直接)
 *      ├─ 物理 → L2 physics_menu (3项)
 *      └─ 化学 → L2 chemistry_menu (7项)
 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/features/education.h"
#include "domain/system/package_manager.h"

namespace tmoe::ui::menus {

// ── L3: 高考真题 (4项) ──────────────────────────────────

static std::shared_ptr<IUIMenu> build_gaokao_papers_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.gaokao_papers_title"), _("edu.gaokao_papers"), "plugin_edu_gaokao_papers");

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2020 (") + _("edu.size_79mb") + ")", "1",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2020",
                "2020", "2020年高考真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2008-2019 (") + _("edu.no_listening") + ", 392.2MiB)", "2",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2019",
                "2019", "2008-2019高考真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2013-2018 (") + _("edu.science_edition") + ", 146.3MiB)", "3",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/gaokao_paper_2013_to_2018",
                "2018", "2013-2018高考真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2008-2018 (") + _("edu.english_listening_only") + ", 244.9MiB)", "4",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/gaokao_english_listening",
                "2018", "2008-2018高考英语听力.tar.xz");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L3: 高考学习笔记 (2项) ──────────────────────────────

static std::shared_ptr<IUIMenu> build_gaokao_notes_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.gaokao_notes_title"), _("edu.gaokao_notes"), "plugin_edu_gaokao_notes");

    menu->add_child(std::make_shared<LambdaAction>(
        std::string(_("edu.gaokao_bio_notes")) + " (131.8MiB)", "1",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/biology_note",
                "2019", "生物笔记.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string(_("edu.gaokao_eng_notes")) + " (5.4MiB)", "2",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/english_note",
                "2020", "英语终极笔记.tar.xz");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L2: 高考 (2个子菜单容器) ────────────────────────────

static std::shared_ptr<IUIMenu> build_gaokao_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.gaokao_title"), _("edu.gaokao_prompt"), "plugin_edu_gaokao");

    menu->add_child(build_gaokao_papers_menu(mgr));
    menu->add_child(build_gaokao_notes_menu(mgr));

    add_navigation_items(menu);
    return menu;
}

// ── L2: 考研 (3项) ──────────────────────────────────────

static std::shared_ptr<IUIMenu> build_kaoyan_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.kaoyan_title"), _("edu.kaoyan_prompt"), "plugin_edu_kaoyan");

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2003-2019 ") + _("edu.kaoyan_politics") + " (6.2MiB)", "1",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/postgraduate_politics",
                "2019", "2003-2019政治真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2001-2019 ") + _("edu.kaoyan_english") + " (7.7MiB)", "2",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/postgraduate_english",
                "2019", "2001-2019英语真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("1987-2020 ") + _("edu.kaoyan_math") + " (" + _("edu.with_solutions") + ", 15.5MiB)", "3",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/postgraduate_math",
                "2020", "1987-2020数学真题.tar.xz");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L2: 数学软件 (5项) ──────────────────────────────────

static std::shared_ptr<IUIMenu> build_math_menu(domain::EducationManager* /*mgr*/) {
    auto menu = make_plugin_menu(
        _("edu.math_title"), _("edu.math_prompt"), "plugin_edu_math");

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.geogebra"), "1",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("geogebra", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.octave"), "2",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("octave", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.scilab"), "3",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"scilab-minimal-bin", "scilab"}, fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.freemat"), "4",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"freemat", "freemat-help"}, fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.maxima"), "5",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"maxima", "wxmaxima"}, fam);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L3: GoldenDict (2项) ────────────────────────────────

static std::shared_ptr<IUIMenu> build_goldendict_menu(domain::EducationManager* /*mgr*/) {
    auto menu = make_plugin_menu(
        _("edu.goldendict_title"), _("edu.goldendict"), "plugin_edu_goldendict");

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.goldendict_install"), "1",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install({"goldendict", "goldendict-wordnet"}, fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.goldendict_dicts"), "2",
        [](MenuContext&) -> bool {
            Logger::warn(_("edu.dict_copyright_notice"));
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L3: 四六级真题 (1项) ────────────────────────────────

static std::shared_ptr<IUIMenu> build_cet_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.cet_title"), _("edu.cet_title"), "plugin_edu_cet");

    menu->add_child(std::make_shared<LambdaAction>(
        std::string("2013-2019 (") + _("edu.cet_both") + ", 6.7MiB)", "1",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/cet",
                "2019", "cet.tar.xz");
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L2: 英语 (2个子菜单 + 1个直接下载, 3项容器) ─────────

static std::shared_ptr<IUIMenu> build_english_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.english_title"), _("edu.english_prompt"), "plugin_edu_english");

    menu->add_child(build_goldendict_menu(mgr));
    menu->add_child(build_cet_menu(mgr));

    menu->add_child(std::make_shared<LambdaAction>(
        std::string(_("edu.english_masterpieces")) + " (222.8MiB)", "3",
        [mgr](MenuContext&) -> bool {
            mgr->download_study_materials(
                "https://gitee.com/ak2/masterpieces",
                "2018", "英文原著.tar.xz");
            Logger::press_enter();
            return true;
        }));

    add_navigation_items(menu);
    return menu;
}

// ── L2: 物理 (3项) ──────────────────────────────────────

static std::shared_ptr<IUIMenu> build_physics_menu(domain::EducationManager* /*mgr*/) {
    auto menu = make_plugin_menu(
        _("edu.physics_title"), _("edu.physics_prompt"), "plugin_edu_physics");

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.step"), "1",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            Logger::info(_("edu.step_desc"));
            domain::PackageManager::install("step", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.openfoam"), "2",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("openfoam", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.geant4"), "3",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("geant321", fam);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L2: 化学 (7项) ──────────────────────────────────────

static std::shared_ptr<IUIMenu> build_chemistry_menu(domain::EducationManager* /*mgr*/) {
    auto menu = make_plugin_menu(
        _("edu.chemistry_title"), _("edu.chemistry_prompt"), "plugin_edu_chemistry");

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.kalzium"), "1",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("kalzium", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.nwchem"), "2",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("nwchem", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.avogadro"), "3",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("avogadro", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.pymol"), "4",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("pymol", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.psi4"), "5",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("psi4", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.gromacs"), "6",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("gromacs", fam);
            Logger::press_enter();
            return true;
        }));

    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.cp2k"), "7",
        [](MenuContext&) -> bool {
            auto fam = domain::PackageManager::detect_distro_family();
            domain::PackageManager::install("cp2k", fam);
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

// ── L1: 科学与教育主菜单 ────────────────────────────────

std::shared_ptr<IUIMenu> create_education_menu(domain::EducationManager* mgr) {
    auto menu = make_plugin_menu(
        _("edu.menu_title"), _("edu.menu_prompt"), "plugin_education");

    // 高考 → L2 容器菜单 (真题 + 学习笔记)
    auto gaokao_menu = build_gaokao_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.gaokao"), "edu_gaokao",
        [gaokao_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(gaokao_menu);
            return true;
        }));

    // 考研 → L2 叶子菜单 (政治/英语/数学真题)
    auto kaoyan_menu = build_kaoyan_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.kaoyan"), "edu_kaoyan",
        [kaoyan_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(kaoyan_menu);
            return true;
        }));

    // 数学 → L2 叶子菜单 (5款数学软件)
    auto math_menu = build_math_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.math"), "edu_math",
        [math_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(math_menu);
            return true;
        }));

    // 英语 → L2 容器菜单 (GoldenDict + 四六级 + 名著)
    auto english_menu = build_english_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.english"), "edu_english",
        [english_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(english_menu);
            return true;
        }));

    // 物理 → L2 叶子菜单 (3款物理软件)
    auto physics_menu = build_physics_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.physics"), "edu_physics",
        [physics_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(physics_menu);
            return true;
        }));

    // 化学 → L2 叶子菜单 (7款化学软件)
    auto chemistry_menu = build_chemistry_menu(mgr);
    menu->add_child(std::make_shared<LambdaAction>(
        _("edu.chemistry"), "edu_chemistry",
        [chemistry_menu](MenuContext& ctx) -> bool {
            MenuEngine(ctx).run(chemistry_menu);
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
