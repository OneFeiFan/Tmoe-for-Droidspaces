#ifndef DOWNLOAD_TOOLS_H
#define DOWNLOAD_TOOLS_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class DownloadTools {
public:
    explicit DownloadTools(const TmoeConfig& cfg);

    // ── 插件子菜单所需的细粒度操作 ──────────────────────
    void configure_aria2();
    void start_aria2();
    void stop_aria2();
    void install_aria_webui();

    // 视频下载器子项
    void install_yt_dlp();
    void install_you_get();
    void install_lux();
    void install_annie();
    void install_gallery_dl();

    // 爬虫工具子项
    void install_httrack();
    void show_wget_mirror_info();
    void install_aria2_batch();
    void install_scrapy();
    void install_curl_batch();

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
