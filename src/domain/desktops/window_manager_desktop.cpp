#include "window_manager_desktop.h"
#include "core/logger.h"

namespace tmoe::domain {

WindowManagerDesktop::WindowManagerDesktop(
    const TmoeConfig& cfg, const DesktopInfo& info)
    : DesktopBase(cfg), info_(info) {}

std::string WindowManagerDesktop::get_id() const {
    return info_.id;
}

const DesktopInfo& WindowManagerDesktop::get_info() const {
    return info_;
}

void WindowManagerDesktop::will_be_installed_message() const {
    Logger::info("WM: " + info_.session_cmd1 +
                 (info_.session_cmd2.empty() ? "" : " / " + info_.session_cmd2));
}

} // namespace tmoe::domain
