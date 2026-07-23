#ifndef EDUCATION_H
#define EDUCATION_H
#pragma once

#include "core/config.h"
#include <functional>
#include <string>

namespace tmoe::domain {

/** 科学与教育管理器
 *
 *  提供学习资料下载（高考/考研/四六级真题、学习笔记、英文名著）
 *  和 whiptail 对话框工具。菜单层级已迁移至 education_plugin。
 */
    class EducationManager {
    public:
        explicit EducationManager(const TmoeConfig &cfg);

        // ═══ 下载模型 (复刻 Bash check_tmoe_study_materials, public for UI plugin dispatch) ═══

        /** 核心下载模型：检测已有 → 解压/重下 → git clone → tar -Jxvf */
        void download_study_materials(const std::string &repo_url,
                                      const std::string &branch,
                                      const std::string &filename);

        /** whiptail yes/no 对话框 */
        bool whiptail_yesno(const std::string &title,
                            const std::string &yes_btn,
                            const std::string &no_btn,
                            const std::string &prompt,
                            int height = 0, int width = 50);

    private:
        const TmoeConfig &cfg_;
        std::string download_folder_;

        /** 获取下载目录 (WSL → /mnt/c/Users/Public/Documents, 否则 ~/sd/Download/Documents) */
        std::string get_download_folder() const;
    };

} // namespace tmoe::domain
#endif
