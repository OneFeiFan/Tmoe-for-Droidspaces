#include "software_center.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

namespace tmoe::domain {

SoftwareCenter::SoftwareCenter(const TmoeConfig& cfg) : cfg_(cfg) {}

void SoftwareCenter::run_software_center_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.menu_title")
            + "\" --menu \"" + _("swcenter.menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("swcenter.sns") + "\" "
            "\"2\" \"" + _("swcenter.games") + "\" "
            "\"3\" \"" + _("swcenter.media") + "\" "
            "\"4\" \"" + _("swcenter.docs") + "\" "
            "\"5\" \"" + _("swcenter.fileshare") + "\" "
            "\"6\" \"" + _("swcenter.pkg_gui") + "\" "
            "\"7\" \"" + _("swcenter.cleanup") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;
        if (ch == "1") run_sns_menu();
        else if (ch == "2") run_games_menu();
        else if (ch == "3") run_media_menu();
        else if (ch == "4") run_docs_menu();
        else if (ch == "5") run_fileshare_menu();
        else if (ch == "6") run_pkg_gui_menu();
        else if (ch == "7") run_cleanup_menu();
        Logger::press_enter();
    }
}

void SoftwareCenter::run_sns_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.sns_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("swcenter.qq") + "\" "
        "\"2\" \""  + _("swcenter.wechat") + "\" "
        "\"3\" \""  + _("swcenter.thunderbird") + "\" "
        "\"4\" \""  + _("swcenter.kmail") + "\" "
        "\"5\" \""  + _("swcenter.evolution") + "\" "
        "\"6\" \""  + _("swcenter.empathy") + "\" "
        "\"7\" \""  + _("swcenter.pidgin") + "\" "
        "\"8\" \""  + _("swcenter.xchat") + "\" "
        "\"9\" \""  + _("swcenter.skype") + "\" "
        "\"10\" \"" + _("swcenter.mitalk") + "\" "
        "\"11\" \"" + _("swcenter.baidunetdisk") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1")      PackageManager::install("linuxqq", family);
    else if (ch == "2") PackageManager::install("wechat", family);
    else if (ch == "3") PackageManager::install("thunderbird", family);
    else if (ch == "4") PackageManager::install("kmail", family);
    else if (ch == "5") PackageManager::install("evolution", family);
    else if (ch == "6") PackageManager::install("empathy", family);
    else if (ch == "7") PackageManager::install("pidgin", family);
    else if (ch == "8") PackageManager::install("xchat", family);
    else if (ch == "9") PackageManager::install("skypeforlinux", family);
    else if (ch == "10") PackageManager::install("mitalk", family);
    else if (ch == "11") PackageManager::install("baidunetdisk", family);
}

void SoftwareCenter::run_games_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.games_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \"" + _("swcenter.steam") + "\" "
        "\"2\" \"" + _("swcenter.retroarch") + "\" "
        "\"3\" \"" + _("swcenter.dolphin_emu") + "\" "
        "\"4\" \"" + _("swcenter.cataclysm") + "\" "
        "\"5\" \"" + _("swcenter.wesnoth") + "\" "
        "\"6\" \"" + _("swcenter.supertuxkart") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1")      PackageManager::install("steam-launcher", family);
    else if (ch == "2") PackageManager::install("retroarch", family);
    else if (ch == "3") PackageManager::install("dolphin-emu", family);
    else if (ch == "4") PackageManager::install("cataclysm-dda", family);
    else if (ch == "5") PackageManager::install("wesnoth", family);
    else if (ch == "6") PackageManager::install("supertuxkart", family);
}

void SoftwareCenter::run_media_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.media_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \""  + _("swcenter.gimp") + "\" "
        "\"2\" \""  + _("swcenter.kolourpaint") + "\" "
        "\"3\" \""  + _("swcenter.peek") + "\" "
        "\"4\" \""  + _("swcenter.parole") + "\" "
        "\"5\" \""  + _("swcenter.clementine") + "\" "
        "\"6\" \""  + _("swcenter.audacity") + "\" "
        "\"7\" \""  + _("swcenter.ardour") + "\" "
        "\"8\" \""  + _("swcenter.mpv") + "\" "
        "\"9\" \""  + _("swcenter.smplayer") + "\" "
        "\"10\" \"" + _("swcenter.obs_studio") + "\" "
        "\"0\" \""  + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1")      PackageManager::install("gimp", family);
    else if (ch == "2") PackageManager::install("kolourpaint", family);
    else if (ch == "3") PackageManager::install("peek", family);
    else if (ch == "4") PackageManager::install("parole", family);
    else if (ch == "5") PackageManager::install("clementine", family);
    else if (ch == "6") PackageManager::install("audacity", family);
    else if (ch == "7") PackageManager::install("ardour", family);
    else if (ch == "8") PackageManager::install("mpv", family);
    else if (ch == "9") PackageManager::install("smplayer", family);
    else if (ch == "10") PackageManager::install("obs-studio", family);
}

void SoftwareCenter::run_docs_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.docs_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \"" + _("swcenter.docs_wps") + "\" "
        "\"2\" \"" + _("swcenter.docs_qq") + "\" "
        "\"3\" \"" + _("swcenter.docs_baidu") + "\" "
        "\"4\" \"" + _("swcenter.docs_thunder") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1")      PackageManager::install("wps-office", family);
    else if (ch == "2") PackageManager::install("linuxqq", family);
    else if (ch == "3") PackageManager::install("baidunetdisk", family);
    else if (ch == "4") {
        // 迅雷没有标准包，尝试安装 aria2 作为替代
        Logger::info(_("swcenter.downloading") + ": Thunder");
        PackageManager::install("aria2", family);
    }
}

void SoftwareCenter::run_fileshare_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.fileshare_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \"" + _("swcenter.filebrowser") + "\" "
        "\"2\" \"" + _("swcenter.nginx_webdav") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1") {
        Logger::info(_("swcenter.downloading") + ": FileBrowser");
        Executor::shell(
            "curl -fsSL https://raw.githubusercontent.com/filebrowser/get/master/get.sh | "
            "bash 2>/dev/null || true"
        );
    } else if (ch == "2") {
        PackageManager::install("nginx", family);
    }
}

void SoftwareCenter::run_pkg_gui_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.pkg_gui_menu")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \"" + _("swcenter.synaptic") + "\" "
        "\"2\" \"" + _("swcenter.gdebi") + "\" "
        "\"3\" \"" + _("swcenter.pamac") + "\" "
        "\"4\" \"" + _("swcenter.bleachbit") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1")      PackageManager::install("synaptic", family);
    else if (ch == "2") PackageManager::install("gdebi", family);
    else if (ch == "3") PackageManager::install("pamac", family);
    else if (ch == "4") PackageManager::install("bleachbit", family);
}

void SoftwareCenter::run_cleanup_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    std::string menu = cfg_.tui_bin + " --title \"" + _("swcenter.cleanup")
        + "\" --menu \"\" 0 0 0 "
        "\"1\" \"" + _("swcenter.cleanup_autoremove") + "\" "
        "\"2\" \"" + _("swcenter.cleanup_purge") + "\" "
        "\"3\" \"" + _("swcenter.bleachbit") + "\" "
        "\"0\" \"" + _("menu.tui.back") + "\"";
    auto ch = Executor::tui_select(menu);
    if (ch == "0" || ch.empty()) return;

    if (ch == "1") {
        Logger::info(_("swcenter.cleanup_running"));
        Executor::shell("apt autoremove -y 2>/dev/null; apt autoclean -y 2>/dev/null || true");
    } else if (ch == "2") {
        std::string pkg_cmd = cfg_.tui_bin + " --title \"" + _("swcenter.cleanup_pkg_name")
            + "\" --inputbox \"" + _("swcenter.cleanup_pkg_prompt") + "\" 0 50";
        auto pkg = Executor::tui_select(pkg_cmd);
        if (!pkg.empty()) PackageManager::remove(pkg, family);
    } else if (ch == "3") {
        PackageManager::install("bleachbit", family);
    }
}

} // namespace tmoe::domain
