#include "domain/features/education.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"

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
    std::string home = SystemHelper::user_home();
    return home + "/sd/Download/Documents";
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

} // namespace tmoe::domain
