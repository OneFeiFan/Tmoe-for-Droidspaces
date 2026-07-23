#pragma once

#include "ui/plugin.h"
#include <memory>

namespace tmoe::domain { class DownloadTools; }

namespace tmoe::ui::menus {

/** 下载工具菜单插件。
 *  3 个嵌套子菜单：Aria2 / 视频下载器 / 爬虫工具。 */
    class DownloadMenuPlugin : public PluginFor<domain::DownloadTools> {
    public:
        using PluginFor::PluginFor;

        std::shared_ptr<IUIMenu> build() override;

    private:
        std::shared_ptr<IUIMenu> build_aria2_menu();

        std::shared_ptr<IUIMenu> build_video_dl_menu();

        std::shared_ptr<IUIMenu> build_crawler_menu();
    };

} // namespace tmoe::ui::menus
