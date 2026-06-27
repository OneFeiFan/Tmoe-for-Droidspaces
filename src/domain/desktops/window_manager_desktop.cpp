#include "window_manager_desktop.h"

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

} // namespace tmoe::domain
