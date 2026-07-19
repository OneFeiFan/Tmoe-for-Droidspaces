#include "domain/apps/media_tools.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include "core/str_utils.h"

namespace tmoe::domain {
    MediaTools::MediaTools(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    // ═══════════════════════════════════════════════════════════
    // 安装/卸载依赖
    // ═══════════════════════════════════════════════════════════
    void MediaTools::install_dependencies() {
        auto family = PackageManager::detect_distro_family();
        std::string img_pkg;
        switch (family) {
            case DistroFamily::Debian: img_pkg = "graphicsmagick-imagemagick-compat";
                break;
            case DistroFamily::RedHat: img_pkg = "ImageMagick";
                break;
            default: img_pkg = "imagemagick";
                break;
        }
        Logger::step(_("media.compress.installing_deps"));
        PackageManager::install(img_pkg, family);
        PackageManager::install("zenity", family);
        Logger::ok(_("media.compress.deps_installed"));
    }

    void MediaTools::remove_dependencies() {
        auto family = PackageManager::detect_distro_family();
        Logger::warn(_("media.compress.remove_warn"));
        if (!Logger::confirm("media.compress.confirm_remove"))
            return;
        PackageManager::remove("imagemagick", family);
        PackageManager::remove("graphicsmagick-imagemagick-compat", family);
        PackageManager::remove("zenity", family);
        Logger::ok(_("media.compress.removed"));
    }

    // ═══════════════════════════════════════════════════════════
    // 压缩入口
    // ═══════════════════════════════════════════════════════════
    void MediaTools::start_compression() {
        if (!check_imagemagick()) {
            install_dependencies();
            if (!check_imagemagick()) {
                Logger::error(_("media.compress.no_convert"));
                return;
            }
        }

        // GUI or TUI?
        if (Executor::has("zenity")) {
            std::string gui_choice = cfg_.tui_bin
                                     + " --title \"GUI or TUI\" --yes-button \"GUI\" --no-button \"TUI\""
                                     + " --yesno \"" + _("media.compress.gui_or_tui") + "\" 0 0";
            auto r = Executor::passthrough(gui_choice);
            if (r.exit_code == 0) {
                gui_compress();
                return;
            }
        }
        tui_compress();
    }

    // ═══════════════════════════════════════════════════════════
    // TUI 压缩
    // ═══════════════════════════════════════════════════════════
    void MediaTools::tui_compress() {
        int quality = choose_quality();

        // 选择图片目录
        std::string start_dir = SystemHelper::user_home();
        if (fs::exists(start_dir + "/图片")) start_dir += "/图片";
        else if (fs::exists(start_dir + "/Pictures")) start_dir += "/Pictures";

        std::string dir_choice = cfg_.tui_bin
                                 + " --inputbox \"" + _("media.compress.choose_dir") + "\" 0 50 " + start_dir
                                 + " 3>&1 1>&2 2>&3";
        auto dir_result = Executor::shell(dir_choice);
        std::string target_dir = dir_result.stdout_data;
        trim_newline(target_dir);

        if (target_dir.empty() || !fs::exists(target_dir)) {
            Logger::error(_("media.compress.dir_not_found"));
            return;
        }

        compress_images(target_dir, quality);
        fix_folder_permissions(target_dir);
    }

    // ═══════════════════════════════════════════════════════════
    // GUI 压缩 (zenity)
    // ═══════════════════════════════════════════════════════════
    void MediaTools::gui_compress() {
        // 用 zenity 选择目录
        auto dir_result = Executor::shell(
            "zenity --file-selection --directory --title=\"" + _("media.compress.choose_dir") + "\" 2>/dev/null");
        std::string target_dir = dir_result.stdout_data;
        trim_newline(target_dir);

        if (target_dir.empty() || !fs::exists(target_dir)) {
            Logger::error(_("media.compress.dir_not_found"));
            return;
        }

        // 用 zenity scale 选择质量
        auto q_result = Executor::shell(
            "zenity --scale --title=\"" + _("media.compress.quality_prompt")
            + "\" --text=\"" + _("media.compress.quality_prompt")
            + "\" --min-value=1 --max-value=99 --value=80 --step=1 2>/dev/null");
        std::string q_str = q_result.stdout_data;
        trim_newline(q_str);
        int quality = q_str.empty() ? 80 : std::stoi(q_str);

        compress_images(target_dir, quality);
        fix_folder_permissions(target_dir);
    }

    // ═══════════════════════════════════════════════════════════
    // 核心压缩逻辑 — 用 ImageMagick convert
    // ═══════════════════════════════════════════════════════════
    void MediaTools::compress_images(const fs::path &dir, int quality) {
        std::string out_dir = "tmoe_compression_quality_" + std::to_string(quality);
        fs::path out_path = dir / out_dir;

        Logger::step(_("media.compress.compressing"));
        Executor::shell("mkdir -p " + out_path.string());

        // 批量转换：遍历所有 jpg/png，执行 convert
        std::string cmd =
                "cd " + dir.string() + " && "
                "for f in *.jpg *.jpeg *.png *.JPG *.JPEG *.PNG; do "
                "  [ -f \"$f\" ] || continue; "
                "  convert +profile \"*\" -strip -quality " + std::to_string(quality)
                + " \"$f\" \"" + out_path.string() + "/${f%%.*}.jpg\" 2>/dev/null; "
                "done";
        Executor::passthrough(cmd);

        Logger::ok(_f("media.compress.done", out_path.string()));

        // 显示结果并尝试打开文件夹
        Executor::shell("ls -lah \"" + out_path.string() + "\"");
        Executor::passthrough("xdg-open \"" + out_path.string() + "\" 2>/dev/null || true");
    }

    // ═══════════════════════════════════════════════════════════
    // 辅助函数
    // ═══════════════════════════════════════════════════════════
    bool MediaTools::check_imagemagick() {
        return Executor::has("convert");
    }

    int MediaTools::choose_quality() {
        std::string cmd = cfg_.tui_bin
                          + " --inputbox \"" + _("media.compress.quality_prompt") + "\" 0 50 80"
                          + " 3>&1 1>&2 2>&3";
        auto result = Executor::shell(cmd);
        std::string q_str = result.stdout_data;
        trim_newline(q_str);
        return q_str.empty() ? 80 : std::stoi(q_str);
    }

    void MediaTools::fix_folder_permissions(const fs::path &dir) {
        // 如果非 root 运行，chown 输出目录给当前用户
#ifndef _WIN32
    if (geteuid() != 0) {
        std::string out_pattern = dir.string() + "/tmoe_compression_quality_*";
        Executor::shell("sudo chown -R $(id -u):$(id -g) " + out_pattern + " 2>/dev/null || true");
    }
#endif
    }
} // namespace tmoe::domain
