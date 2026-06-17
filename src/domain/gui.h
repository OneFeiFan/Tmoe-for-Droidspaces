#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {
    /// GUI / VNC 桌面环境管理
    /// 对应 Bash: tools/gui/gui, tools/gui/startvnc, tools/gui/novnc
    class GUIManager {
    public:
        explicit GUIManager(const TmoeConfig &cfg) : cfg_(cfg) {
        }

        /// 安装桌面环境（xfce4/lxde/mate/kde/fluxbox）
        bool install_desktop(std::string_view desktop);

        /// 启动 VNC 服务
        bool start_vnc(int display = 1, int width = 1280, int height = 720);

        /// 启动 noVNC（Web 客户端）
        bool start_novnc(int port = 6080);

        /// 停止 VNC
        bool stop_vnc(int display = 1);

        /// 启动宿主机 PulseAudio 桥接守护进程
        bool start_pulseaudio_bridge() const;

    private:
        const TmoeConfig &cfg_;
    };
} // namespace tmoe::domain
