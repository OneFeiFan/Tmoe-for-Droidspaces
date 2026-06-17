#include "domain/gui.h"
#include "core/executor.h"
#include "core/logger.h"

namespace tmoe::domain {
    bool GUIManager::install_desktop(std::string_view desktop) {
        Logger::step("安装桌面环境: " + std::string(desktop));
        // TODO: 在容器内 apt install xfce4 / lxde ...
        return true;
    }

    bool GUIManager::start_vnc(int display, int width, int height) {
        Logger::step("启动 VNC 服务 (display " + std::to_string(display) + ")");
        // TODO: 启动 x11vnc / tigervnc
        return true;
    }

    bool GUIManager::start_novnc(int port) {
        Logger::step("启动 noVNC (port " + std::to_string(port) + ")");
        // TODO: 启动 websockify + noVNC
        return true;
    }

    bool GUIManager::stop_vnc(int display) {
        Logger::step("停止 VNC (display " + std::to_string(display) + ")");
        return true;
    }

    bool GUIManager::start_pulseaudio_bridge() const {
        if (!Executor::has("pulseaudio")) {
            Logger::warn("宿主机未安装 PulseAudio，将跳过声音桥接初始化");
            return false;
        }

        Logger::step("正在拉起 PulseAudio TCP 桥接守护进程...");

        // Linux Root 用户运行 pulseaudio 会被拒绝，降级执行；Termux 则直接执行
        std::string prefix = "";
        if (!cfg_.is_termux && std::getenv("SUDO_USER")) {
            prefix = std::string("su - ") + std::getenv("SUDO_USER") + " -c ";
        }

        // 复刻原版桥接参数：开启 TCP 模块，允许本地匿名访问，并且取消自动空闲退出
        std::string cmd = prefix + "\"pulseaudio --start --load=\\\"module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1\\\" --exit-idle-time=-1\"";

        return Executor::shell(cmd + " >/dev/null 2>&1").ok();
    }
} // namespace tmoe::domain
