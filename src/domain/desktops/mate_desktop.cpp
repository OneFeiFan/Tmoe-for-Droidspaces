#include "mate_desktop.h"
#include "desktop_utils.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

MateDesktop::MateDesktop(const TmoeConfig& cfg)
    : DesktopBase(cfg)
    , info_(gui_config::all_desktops()[4])  // mate is idx 4
{}

std::string MateDesktop::get_id() const        { return info_.id; }
const DesktopInfo& MateDesktop::get_info() const { return info_; }

void MateDesktop::will_be_installed_message() const {
    Logger::info("MATE: mate-session / mate-panel");
}

// ── 阶段2: mate/ubuntu-mate + core/lite + Linux Mint ──
PreInstallChoices MateDesktop::pre_install_choices(
    DistroFamily family, bool is_auto_mode) {

    PreInstallChoices c;
    if (is_auto_mode || family != DistroFamily::Debian) return c;

    // Linux Mint 优先
    auto issue = SystemHelper::read_file("/etc/issue");
    if (issue.find("Linux Mint") != std::string::npos) {
        c.pkg_list = "mint-meta-mate mint-meta-core mint-artwork";
        return c;
    }

    bool is_ubuntu = (cfg_.sub_distro == "ubuntu");
    if (is_ubuntu) {
        auto r = Executor::passthrough(cfg_.tui_bin +
            " --title \"Mate or Ubuntu-MATE\""
            " --yes-button \"mate\" --no-button \"ubuntu-mate\""
            " --yesno '前者为普通mate,后者为ubuntu-mate' 0 0");
        if (r.exit_code == 1) { c.pkg_list = "ubuntu-mate-desktop"; return c; }
    }

    // core / lite
    auto r = Executor::passthrough(cfg_.tui_bin +
        " --title \"MATE-CORE or MATE-LITE\""
        " --yes-button \"core\" --no-button \"lite\""
        " --yesno '前者为普通mate,后者为精简版mate' 0 0");
    if (r.exit_code != 0) {
        c.use_no_recommends = true;
        c.pkg_list = "mate-session-manager mate-settings-daemon marco mate-terminal mate-panel";
    }
    return c;
}

// ── 阶段3: fonts + arch warning + apt clean ──
void MateDesktop::post_install_config(const PostInstallContext& ctx) {
    if (ctx.family == DistroFamily::Arch && ctx.is_proot) {
        auto r = Executor::passthrough(cfg_.tui_bin +
            " --title \"MATE on Arch proot\""
            " --yes-button \"continue\" --no-button \"switch\""
            " --yesno 'MATE may flicker in Arch proot. Continue anyway?\\n[No]=switch to LXDE/LXQt/XFCE' 0 0");
        if (r.exit_code != 0) {
            Logger::warn("MATE install aborted for Arch proot — try lxde, lxqt, or xfce instead");
            return;
        }
    }
    desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
    desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
    // fonts + clean
    if (ctx.is_debian) {
        desktop_utils::install_noto_fonts(ctx.family, true);
        Executor::passthrough("apt clean 2>/dev/null || true");
        Executor::passthrough("apt autoclean 2>/dev/null || true");
    }
    desktop_utils::install_language_packs(cfg_);
}

} // namespace tmoe::domain
