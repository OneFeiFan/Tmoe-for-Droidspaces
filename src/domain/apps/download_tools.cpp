#include "domain/apps/download_tools.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"

namespace tmoe::domain {
    DownloadTools::DownloadTools(const TmoeConfig &cfg) : cfg_(cfg) {
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
        CommandBuilder("mkdir").add_flag("-p").add_arg("~/.aria2").add_raw("2>/dev/null").execute();
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
        Logger::ok(_("download.aria2_started"));
    }

    void DownloadTools::stop_aria2() {
        Logger::step(_("download.aria2_stop"));
        CommandBuilder("pkill").add_arg("aria2c").add_raw("2>/dev/null || true").execute();
        Logger::ok(_("download.aria2_stopped"));
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

    // ── 插件子菜单所需的细粒度操作 ──────────────────────

    // 视频下载器子项

    void DownloadTools::install_yt_dlp() {
        Logger::step(_("download.yt_dlp_pip"));
        Executor::shell("pip3 install yt-dlp 2>/dev/null || "
            "pip install yt-dlp 2>/dev/null || "
            "sudo apt install -y yt-dlp 2>/dev/null || "
            "sudo pacman -S --noconfirm yt-dlp 2>/dev/null || true");
        Logger::info("  " + _("download.yt_dlp_example"));
    }

    void DownloadTools::install_you_get() {
        Logger::step(_("download.you_get"));
        Executor::shell("pip3 install you-get 2>/dev/null || "
            "pip install you-get 2>/dev/null || true");
        Logger::info("  " + _("download.you_get_example"));
    }

    void DownloadTools::install_lux() {
        Logger::step(_("download.lux"));
        Executor::shell(
            "if command -v go >/dev/null 2>&1; then "
            "  go install github.com/iawia002/lux@latest 2>/dev/null; "
            "elif [ -f /usr/local/bin/lux ]; then :; "
            "else "
            "  sudo wget -q https://github.com/iawia002/lux/releases/latest/download/"
            "    lux_$(uname -s)_$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
            "    -O /usr/local/bin/lux 2>/dev/null && "
            "  sudo chmod 755 /usr/local/bin/lux 2>/dev/null; "
            "fi"
        );
        Logger::info("  " + _("download.lux_example"));
    }

    void DownloadTools::install_annie() {
        Logger::step(_("download.annie"));
        Executor::shell(
            "if command -v go >/dev/null 2>&1; then "
            "  go install github.com/iawia002/annie@latest 2>/dev/null; "
            "else "
            "  sudo wget -q https://github.com/iawia002/annie/releases/latest/download/"
            "    annie_$(uname -s)_$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
            "    -O /usr/local/bin/annie 2>/dev/null && "
            "  sudo chmod 755 /usr/local/bin/annie 2>/dev/null; "
            "fi"
        );
    }

    void DownloadTools::install_gallery_dl() {
        Logger::step(_("download.gallery_dl"));
        Executor::shell("pip3 install gallery-dl 2>/dev/null || "
            "pip install gallery-dl 2>/dev/null || true");
    }

    // 爬虫工具子项

    void DownloadTools::install_httrack() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("download.crawler_installing"));
        PackageManager::install("webhttrack", family);
        Logger::info("  " + _("download.httrack_example"));
    }

    void DownloadTools::show_wget_mirror_info() {
        Logger::info("  " + _("download.wget_mirror_example"));
    }

    void DownloadTools::install_aria2_batch() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("download.crawler_installing"));
        PackageManager::install("aria2", family);
    }

    void DownloadTools::install_scrapy() {
        Logger::step(_("download.crawler_installing"));
        Executor::shell("pip3 install scrapy 2>/dev/null || "
            "pip install scrapy 2>/dev/null || true");
    }

    void DownloadTools::install_curl_batch() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("download.crawler_installing"));
        PackageManager::install("curl", family);
    }
} // namespace tmoe::domain
