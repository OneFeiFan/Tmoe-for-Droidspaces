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
#include "ui/menus/education_plugin.h"

namespace tmoe::ui::menus {

// ── L3: 高考真题 (4项) ──────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_gaokao_papers_menu() {
        auto menu = make_plugin_menu(
                _("edu.gaokao_papers_title"), _("edu.gaokao_papers"), "plugin_edu_gaokao_papers");

        menu->add_action(
                std::string("2020 (") + _("edu.size_79mb") + ")", "1",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/gaokao_paper_2020",
                            "2020", "2020年高考真题.tar.xz");
                });

        menu->add_action(
                std::string("2008-2019 (") + _("edu.no_listening") + ", 392.2MiB)", "2",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/gaokao_paper_2019",
                            "2019", "2008-2019高考真题.tar.xz");
                });

        menu->add_action(
                std::string("2013-2018 (") + _("edu.science_edition") + ", 146.3MiB)", "3",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/gaokao_paper_2013_to_2018",
                            "2018", "2013-2018高考真题.tar.xz");
                });

        menu->add_action(
                std::string("2008-2018 (") + _("edu.english_listening_only") + ", 244.9MiB)", "4",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/gaokao_english_listening",
                            "2018", "2008-2018高考英语听力.tar.xz");
                });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L3: 高考学习笔记 (2项) ──────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_gaokao_notes_menu() {
        auto menu = make_plugin_menu(
                _("edu.gaokao_notes_title"), _("edu.gaokao_notes"), "plugin_edu_gaokao_notes");

        menu->add_action(
                std::string(_("edu.gaokao_bio_notes")) + " (131.8MiB)", "1",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/biology_note",
                            "2019", "生物笔记.tar.xz");
                });

        menu->add_action(
                std::string(_("edu.gaokao_eng_notes")) + " (5.4MiB)", "2",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/english_note",
                            "2020", "英语终极笔记.tar.xz");
                });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L2: 高考 (2个子菜单容器) ────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_gaokao_menu() {
        auto menu = make_plugin_menu(
                _("edu.gaokao_title"), _("edu.gaokao_prompt"), "plugin_edu_gaokao");

        menu->add_child(build_gaokao_papers_menu());
        menu->add_child(build_gaokao_notes_menu());

        add_navigation_items(menu);
        return menu;
    }

// ── L2: 考研 (3项) ──────────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_kaoyan_menu() {
        auto menu = make_plugin_menu(
                _("edu.kaoyan_title"), _("edu.kaoyan_prompt"), "plugin_edu_kaoyan");

        menu->add_action(
                std::string("2003-2019 ") + _("edu.kaoyan_politics") + " (6.2MiB)", "1",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/postgraduate_politics",
                            "2019", "2003-2019政治真题.tar.xz");
                });

        menu->add_action(
                std::string("2001-2019 ") + _("edu.kaoyan_english") + " (7.7MiB)", "2",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/postgraduate_english",
                            "2019", "2001-2019英语真题.tar.xz");
                });

        menu->add_action(
                std::string("1987-2020 ") + _("edu.kaoyan_math") + " (" + _("edu.with_solutions") + ", 15.5MiB)", "3",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/postgraduate_math",
                            "2020", "1987-2020数学真题.tar.xz");
                });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L2: 数学软件 (5项) ──────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_math_menu() {
        auto menu = make_plugin_menu(
                _("edu.math_title"), _("edu.math_prompt"), "plugin_edu_math");

        menu->add_action(_("edu.geogebra"), "1",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("geogebra", fam);
                         });

        menu->add_action(_("edu.octave"), "2",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("octave", fam);
                         });

        menu->add_action(_("edu.scilab"), "3",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install({"scilab-minimal-bin", "scilab"}, fam);
                         });

        menu->add_action(_("edu.freemat"), "4",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install({"freemat", "freemat-help"}, fam);
                         });

        menu->add_action(_("edu.maxima"), "5",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install({"maxima", "wxmaxima"}, fam);
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L3: GoldenDict (2项) ────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_goldendict_menu() {
        auto menu = make_plugin_menu(
                _("edu.goldendict_title"), _("edu.goldendict"), "plugin_edu_goldendict");

        menu->add_action(_("edu.goldendict_install"), "1",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install({"goldendict", "goldendict-wordnet"}, fam);
                         });

        menu->add_action(_("edu.goldendict_dicts"), "2",
                         [] { Logger::warn(_("edu.dict_copyright_notice")); });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L3: 四六级真题 (1项) ────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_cet_menu() {
        auto menu = make_plugin_menu(
                _("edu.cet_title"), _("edu.cet_title"), "plugin_edu_cet");

        menu->add_action(
                std::string("2013-2019 (") + _("edu.cet_both") + ", 6.7MiB)", "1",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/cet",
                            "2019", "cet.tar.xz");
                });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L2: 英语 (2个子菜单 + 1个直接下载, 3项容器) ─────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_english_menu() {
        auto menu = make_plugin_menu(
                _("edu.english_title"), _("edu.english_prompt"), "plugin_edu_english");

        menu->add_child(build_goldendict_menu());
        menu->add_child(build_cet_menu());

        menu->add_action(
                std::string(_("edu.english_masterpieces")) + " (222.8MiB)", "3",
                [this] {
                    mgr_->download_study_materials(
                            "https://gitee.com/ak2/masterpieces",
                            "2018", "英文原著.tar.xz");
                });

        add_navigation_items(menu);
        return menu;
    }

// ── L2: 物理 (3项) ──────────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_physics_menu() {
        auto menu = make_plugin_menu(
                _("edu.physics_title"), _("edu.physics_prompt"), "plugin_edu_physics");

        menu->add_action(_("edu.step"), "1",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             Logger::info(_("edu.step_desc"));
                             domain::PackageManager::install("step", fam);
                         });

        menu->add_action(_("edu.openfoam"), "2",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("openfoam", fam);
                         });

        menu->add_action(_("edu.geant4"), "3",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("geant321", fam);
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L2: 化学 (7项) ──────────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build_chemistry_menu() {
        auto menu = make_plugin_menu(
                _("edu.chemistry_title"), _("edu.chemistry_prompt"), "plugin_edu_chemistry");

        menu->add_action(_("edu.kalzium"), "1",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("kalzium", fam);
                         });

        menu->add_action(_("edu.nwchem"), "2",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("nwchem", fam);
                         });

        menu->add_action(_("edu.avogadro"), "3",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("avogadro", fam);
                         });

        menu->add_action(_("edu.pymol"), "4",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("pymol", fam);
                         });

        menu->add_action(_("edu.psi4"), "5",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("psi4", fam);
                         });

        menu->add_action(_("edu.gromacs"), "6",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("gromacs", fam);
                         });

        menu->add_action(_("edu.cp2k"), "7",
                         [] {
                             auto fam = domain::PackageManager::detect_distro_family();
                             domain::PackageManager::install("cp2k", fam);
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ── L1: 科学与教育主菜单 ────────────────────────────────

    std::shared_ptr<IUIMenu> EducationMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("edu.menu_title"), _("edu.menu_prompt"), "plugin_education");

        // 高考 → L2 容器菜单 (真题 + 学习笔记)
        menu->add_submenu(_("edu.gaokao"), "edu_gaokao", build_gaokao_menu());

        // 考研 → L2 叶子菜单 (政治/英语/数学真题)
        menu->add_submenu(_("edu.kaoyan"), "edu_kaoyan", build_kaoyan_menu());

        // 数学 → L2 叶子菜单 (5款数学软件)
        menu->add_submenu(_("edu.math"), "edu_math", build_math_menu());

        // 英语 → L2 容器菜单 (GoldenDict + 四六级 + 名著)
        menu->add_submenu(_("edu.english"), "edu_english", build_english_menu());

        // 物理 → L2 叶子菜单 (3款物理软件)
        menu->add_submenu(_("edu.physics"), "edu_physics", build_physics_menu());

        // 化学 → L2 叶子菜单 (7款化学软件)
        menu->add_submenu(_("edu.chemistry"), "edu_chemistry", build_chemistry_menu());

        add_sandwich_nav(menu);
        return menu;
    }

} // namespace tmoe::ui::menus