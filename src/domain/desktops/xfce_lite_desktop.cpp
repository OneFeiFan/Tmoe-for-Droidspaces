#include "xfce_lite_desktop.h"
#include "core/executor.h"
#include "core/logger.h"
#include <filesystem>

namespace tmoe::domain {

XfceLiteDesktop::XfceLiteDesktop(const TmoeConfig& cfg)
    : XfceDesktop(cfg)
    , info_(gui_config::all_desktops()[1])  // xfce-lite is idx 1
{}

std::string XfceLiteDesktop::get_id() const        { return info_.id; }
const DesktopInfo& XfceLiteDesktop::get_info() const { return info_; }

void XfceLiteDesktop::will_be_installed_message() const {
    Logger::info("XFCE Lite: xfce4-session / startxfce4");
}

// xfce-lite: 跳过壁纸和 papirus 图标，其他和 xfce 一样
void XfceLiteDesktop::post_install_extras(const PostInstallContext& ctx) {
    // 仅保留光标主题
    if (ctx.family != DistroFamily::Alpine) {
        if (!fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) {
            Executor::shell(
                "cd /tmp && "
                "wget -qO breeze-adapta-cursor.tar.gz "
                "'https://mirrors.bfsu.edu.cn/archlinux/community/os/x86_64/"
                "breeze-adapta-cursor-theme-5.90.0-1-any.pkg.tar.zst' 2>/dev/null || "
                "curl -sL 'https://gitee.com/ak2/breeze-adapta-cursor/raw/master/"
                "breeze-adapta-cursor.tar.gz' -o breeze-adapta-cursor.tar.gz 2>/dev/null; "
                "tar -xzf breeze-adapta-cursor.tar.gz -C /usr/share/icons/ 2>/dev/null || true");
        }
    }
    // xfce-lite: 跳过 papirus 图标和壁纸 (bash 原版 xfce_lite 也不设)
}

} // namespace tmoe::domain
