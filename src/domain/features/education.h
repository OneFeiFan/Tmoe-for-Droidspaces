#ifndef EDUCATION_H
#define EDUCATION_H
#pragma once
#include "core/config.h"
#include <functional>
#include <string>

namespace tmoe::domain {

/** 科学与教育管理器 — 完全复刻 Bash: old/tools/app/education (391行)
 *
 *  Bash 菜单层级:
 *  L1 tmoe_education_app_menu (6项)
 *    ├─ 1.高考 → L2 tmoe_college_entrance_examination (2项)
 *    │   ├─ 真题 → L3 college_entrance_examination_paper (4项)
 *    │   └─ 学习笔记 → L3 college_entrance_examination_notes (2项)
 *    ├─ 2.考研 → L2 tmoe_postgraduate_entrance_examination (3项)
 *    ├─ 3.数学 → L2 tmoe_mathematics_menu (5项)
 *    ├─ 4.英语 → L2 tmoe_english_menu (3项)
 *    │   ├─ goldendict → L3 tmoe_golden_dict_menu (2项)
 *    │   ├─ 四六级 → L3 cet4_and_6_exam_paper (1项)
 *    │   └─ 名著 → 直接下载
 *    ├─ 5.物理 → L2 tmoe_physics_menu (3项)
 *    └─ 6.化学 → L2 tmoe_chemistry_menu (7项)
 */
class EducationManager {
public:
    explicit EducationManager(const TmoeConfig& cfg);

    /** 科学与教育主菜单入口 (L1) */
    void run_education_menu();

    // ═══ L1 → L2 子菜单 (public for UI plugin dispatch) ═══

    /** L2: 高考 (真题 / 学习笔记) */
    void run_gaokao_menu();

    /** L2: 考研 (政治/英语/数学真题) */
    void run_kaoyan_menu();

    /** L2: 数学软件 */
    void run_math_menu();

    /** L2: 英语 (词典/四六级/名著) */
    void run_english_menu();

    /** L2: 物理软件 */
    void run_physics_menu();

    /** L2: 化学软件 */
    void run_chemistry_menu();

    // ═══ L3 子菜单 (public for UI plugin dispatch) ═══

    /** L3: 高考真题 — 4个年份版本 */
    void run_gaokao_papers_menu();

    /** L3: 高考学习笔记 — 生物/英语 */
    void run_gaokao_notes_menu();

    /** L3: GoldenDict (安装/词库) */
    void run_goldendict_menu();

    /** L3: 四六级真题 */
    void run_cet_menu();

    // ═══ 下载模型 (复刻 Bash check_tmoe_study_materials, public for UI plugin dispatch) ═══

    /** 核心下载模型：检测已有 → 解压/重下 → git clone → tar -Jxvf */
    void download_study_materials(const std::string& repo_url,
                                  const std::string& branch,
                                  const std::string& filename);

    /** whiptail yes/no 对话框 */
    bool whiptail_yesno(const std::string& title,
                        const std::string& yes_btn,
                        const std::string& no_btn,
                        const std::string& prompt,
                        int height = 0, int width = 50);

private:
    const TmoeConfig& cfg_;
    std::string download_folder_;

    /** 获取下载目录 (WSL → /mnt/c/Users/Public/Documents, 否则 ~/sd/Download/Documents) */
    std::string get_download_folder() const;
};

} // namespace tmoe::domain
#endif
