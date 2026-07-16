#ifndef DOWNLOAD_TOOLS_H
#define DOWNLOAD_TOOLS_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class DownloadTools {
public:
    explicit DownloadTools(const TmoeConfig& cfg);
    void run_download_menu();

    // 子菜单入口（供 UI 插件直接调用）
    void run_aria2_menu();
    void run_video_dl_menu();
    void run_crawler_menu();

private:
    void install_aria2();
    void configure_aria2();
    void start_aria2();
    void stop_aria2();
    void install_aria_webui();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
