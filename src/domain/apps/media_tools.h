#pragma once

#include "core/config.h"
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace tmoe::domain {

/** 批量压缩图片工具 — 对应 Bash old/tools/optimization/compress_pictures */
    class MediaTools {
    public:
        explicit MediaTools(const TmoeConfig &cfg);

        // ── 子菜单选项 ──
        void start_compression();

        void install_dependencies();

        void remove_dependencies();

    private:
        // ── 压缩流程 ──
        void tui_compress();

        void gui_compress();

        void compress_images(const fs::path &dir, int quality);

        // ── 辅助 ──
        bool check_imagemagick();

        int choose_quality();

        void fix_folder_permissions(const fs::path &dir);

        const TmoeConfig &cfg_;
    };

} // namespace tmoe::domain
