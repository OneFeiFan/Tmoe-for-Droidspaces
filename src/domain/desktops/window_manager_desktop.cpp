#include "window_manager_desktop.h"
#include "core/logger.h"

namespace tmoe::domain {

    WindowManagerDesktop::WindowManagerDesktop(
            const TmoeConfig &cfg, const DesktopInfo &info)
            : DesktopBase(cfg, info) {}

    void WindowManagerDesktop::will_be_installed_message() const {
        auto &info = get_info();
        Logger::info("WM: " + info.session_cmd1 +
                     (info.session_cmd2.empty() ? "" : " / " + info.session_cmd2));
    }

} // namespace tmoe::domain
