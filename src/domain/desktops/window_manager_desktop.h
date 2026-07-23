#pragma once

#include "desktop_base.h"

namespace tmoe::domain {

/** 统一处理全部窗口管理器（icewm, openbox, i3, etc.）。
 *  通过构造函数传入 DesktopInfo 引用实现参数化，
 *  无需为 50+ WM 各建一个类。 */
    class WindowManagerDesktop : public DesktopBase {
    public:
        WindowManagerDesktop(const TmoeConfig &cfg, const DesktopInfo &info);

        bool is_window_manager() const override { return true; }

        bool needs_root() const override { return false; }

        void will_be_installed_message() const override;
    };

} // namespace tmoe::domain
