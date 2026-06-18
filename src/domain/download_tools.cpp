#include "download_tools.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "package_manager.h"

namespace tmoe::domain {

DownloadTools::DownloadTools(const TmoeConfig& cfg) : cfg_(cfg) {}

void DownloadTools::run_download_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("download.menu_title")
            + "\" --menu \"" + _("download.menu_prompt") + "\" 0 0 0 "
            "\"1\" \"" + _("download.aria2_manager") + "\" "
            "\"2\" \"" + _("download.thunder") + "\" "
            "\"3\" \"" + _("download.video_dl") + "\" "
            "\"4\" \"" + _("download.crawler") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;
        if (ch == "1") run_aria2_menu();
        else if (ch == "2") {
            Logger::info(_("download.thunder_info"));
            Logger::info(_("download.thunder_install"));
            Executor::shell(
                "wget -O /tmp/thunder.exe "
                "https://down.sandai.net/thunder11/XunLeiSetup11.4.8.2110.exe "
                "2>/dev/null || "
                "echo 'https://dl.xunlei.com/' || true"
            );
            Logger::info(_("download.thunder_web") + ": https://dl.xunlei.com/");
        }
        else if (ch == "3") run_video_dl_menu();
        else if (ch == "4") run_crawler_menu();
        Logger::press_enter();
    }
}

void DownloadTools::run_aria2_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("download.aria2_menu")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \"" + _("download.aria2_install") + "\" "
            "\"2\" \"" + _("download.aria2_config") + "\" "
            "\"3\" \"" + _("download.aria2_start") + "\" "
            "\"4\" \"" + _("download.aria2_stop") + "\" "
            "\"5\" \"" + _("download.aria2_webui") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            Logger::step(_("download.aria2_installing"));
            PackageManager::install("aria2", family);
        }
        else if (ch == "2") configure_aria2();
        else if (ch == "3") start_aria2();
        else if (ch == "4") stop_aria2();
        else if (ch == "5") install_aria_webui();
        Logger::press_enter();
    }
}

void DownloadTools::configure_aria2() {
    Logger::step(_("download.aria2_config"));
    std::string aria2_conf = R"(
# tmoe-generated aria2 config
dir=/sdcard/Download
continue=true
max-concurrent-downloads=5
max-connection-per-server=16
min-split-size=10M
split=16
enable-rpc=true
rpc-listen-all=false
rpc-listen-port=6800
rpc-secret=tmoe
listen-port=51413
seed-time=0
bt-tracker=udp://tracker.opentrackr.org:1337/announce,udp://tracker.openbittorrent.com:6969/announce
)";
    Executor::shell("mkdir -p ~/.aria2 2>/dev/null");
    Executor::shell("cat > ~/.aria2/aria2.conf << 'TMOE_EOF'\n" + aria2_conf + "\nTMOE_EOF");
    Logger::ok(_("download.aria2_configured"));
    Logger::info(_("download.aria2_rpc_port"));
}

void DownloadTools::start_aria2() {
    Logger::step(_("download.aria2_start"));
    if (!Executor::shell("command -v aria2c >/dev/null 2>&1")) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install("aria2", family);
    }
    Executor::shell(
        "if ! pgrep -x aria2c >/dev/null 2>&1; then "
        "  nohup aria2c --conf-path=$HOME/.aria2/aria2.conf "
        "    -D >/dev/null 2>&1 & "
        "  sleep 1; fi"
    );
    Logger::ok("aria2c");
}

void DownloadTools::stop_aria2() {
    Logger::step(_("download.aria2_stop"));
    Executor::shell("pkill aria2c 2>/dev/null || true");
    Logger::ok("aria2c");
}

void DownloadTools::install_aria_webui() {
    Logger::step(_("download.aria2_web_installing"));
    Executor::shell(
        "mkdir -p /sdcard/Download/aria2ng 2>/dev/null; "
        "cd /sdcard/Download/aria2ng && "
        "if [ ! -f index.html ]; then "
        "  wget -q --no-check-certificate "
        "    https://github.com/mayswind/AriaNg/releases/download/1.3.6/AriaNg-1.3.6.zip "
        "    -O aria2ng.zip 2>/dev/null && "
        "  unzip -o aria2ng.zip 2>/dev/null && "
        "  rm -f aria2ng.zip; fi"
    );
    Logger::ok(_("download.aria2_web_done"));
}

void DownloadTools::run_video_dl_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("download.video_dl")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \"" + _("download.yt_dlp") + "\" "
            "\"2\" \"" + _("download.you_get") + "\" "
            "\"3\" \"" + _("download.lux") + "\" "
            "\"4\" \"" + _("download.annie") + "\" "
            "\"5\" \"" + _("download.gallery_dl") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            Logger::step(_("download.yt_dlp_pip"));
            Executor::shell("pip3 install yt-dlp 2>/dev/null || "
                           "pip install yt-dlp 2>/dev/null || "
                           "apt install -y yt-dlp 2>/dev/null || "
                           "pacman -S --noconfirm yt-dlp 2>/dev/null || true");
            Logger::info("  " + _("download.yt_dlp_example"));
        }
        else if (ch == "2") {
            Logger::step(_("download.you_get"));
            Executor::shell("pip3 install you-get 2>/dev/null || "
                           "pip install you-get 2>/dev/null || true");
            Logger::info("  " + _("download.you_get_example"));
        }
        else if (ch == "3") {
            Logger::step(_("download.lux"));
            Executor::shell(
                "if command -v go >/dev/null 2>&1; then "
                "  go install github.com/iawia002/lux@latest 2>/dev/null; "
                "elif [ -f /usr/local/bin/lux ]; then :; "
                "else "
                "  wget -q https://github.com/iawia002/lux/releases/latest/download/"
                "    lux_$(uname -s)_$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
                "    -O /usr/local/bin/lux 2>/dev/null && "
                "  chmod 755 /usr/local/bin/lux 2>/dev/null; "
                "fi"
            );
            Logger::info("  " + _("download.lux_example"));
        }
        else if (ch == "4") {
            Logger::step(_("download.annie"));
            Executor::shell(
                "if command -v go >/dev/null 2>&1; then "
                "  go install github.com/iawia002/annie@latest 2>/dev/null; "
                "else "
                "  wget -q https://github.com/iawia002/annie/releases/latest/download/"
                "    annie_$(uname -s)_$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
                "    -O /usr/local/bin/annie 2>/dev/null && "
                "  chmod 755 /usr/local/bin/annie 2>/dev/null; "
                "fi"
            );
        }
        else if (ch == "5") {
            Logger::step(_("download.gallery_dl"));
            Executor::shell("pip3 install gallery-dl 2>/dev/null || "
                           "pip install gallery-dl 2>/dev/null || true");
        }
        Logger::press_enter();
    }
}

void DownloadTools::run_crawler_menu() {
    auto family = infer_family_from_config(cfg_.linux_distro);
    while (true) {
        std::string menu = cfg_.tui_bin + " --title \"" + _("download.crawler")
            + "\" --menu \"\" 0 0 0 "
            "\"1\" \"" + _("download.crawler_httrack") + "\" "
            "\"2\" \"" + _("download.crawler_wget_mirror") + "\" "
            "\"3\" \"" + _("download.crawler_aria2_batch") + "\" "
            "\"4\" \"" + _("download.crawler_scrapy") + "\" "
            "\"5\" \"" + _("download.crawler_curl_batch") + "\" "
            "\"0\" \"" + _("menu.tui.back_upper") + "\"";

        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) break;

        if (ch == "1") {
            Logger::step(_("download.crawler_installing"));
            PackageManager::install("webhttrack", family);
            Logger::info("  " + _("download.httrack_example"));
        }
        else if (ch == "2") {
            Logger::info("  " + _("download.wget_mirror_example"));
        }
        else if (ch == "3") {
            Logger::step(_("download.crawler_installing"));
            PackageManager::install("aria2", family);
        }
        else if (ch == "4") {
            Logger::step(_("download.crawler_installing"));
            Executor::shell("pip3 install scrapy 2>/dev/null || "
                           "pip install scrapy 2>/dev/null || true");
        }
        else if (ch == "5") {
            Logger::step(_("download.crawler_installing"));
            PackageManager::install("curl", family);
        }
        Logger::press_enter();
    }
}

} // namespace tmoe::domain
