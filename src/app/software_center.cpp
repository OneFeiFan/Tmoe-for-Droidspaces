#include "software_center.h"

namespace tmoe::app {
    std::vector<std::string> SoftwareCenter::available() const {
        return {
            "vscode", "firefox", "chromium", "wps",
            "libreoffice", "gimp", "vlc", "obs-studio",
            "nodejs", "python", "golang", "rust"
        };
    }

    bool SoftwareCenter::install(std::string_view name) {
        Logger::step("安装: " + std::string(name));
        // TODO: 在容器内运行 apt/pacman/pkg install
        return true;
    }

    bool SoftwareCenter::remove(std::string_view name) {
        Logger::step("卸载: " + std::string(name));
        // TODO: 在容器内执行 apt/pacman/pkg remove
        return true;
    }
} // namespace tmoe::app
