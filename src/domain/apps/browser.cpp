#include "domain/apps/browser.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"
#include <fstream>

namespace tmoe::domain {
    BrowserManager::BrowserManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void BrowserManager::run_browser_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"Browsers\""
                               " --menu \"Which browser do you want to install?\" 0 50 0 "
                               "\"1\" \"Mozilla Firefox & Google Chromium\" "
                               "\"2\" \"Microsoft Edge (x64,享受出色的浏览体验)\" "
                               "\"3\" \"Falkon (Qupzilla的前身,来自KDE,使用QtWebEngine)\" "
                               "\"4\" \"Vivaldi (一切皆可定制)\" "
                               "\"5\" \"Epiphany (GNOME默认浏览器,基于Mozilla的Gecko)\" "
                               "\"6\" \"Midori (轻量级,开源浏览器)\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;

            if (ch == "1") firefox_or_chromium();
            else if (ch == "2") microsoft_edge_menu();
            else if (ch == "3") falkon_browser_menu();
            else if (ch == "4") install_vivaldi();
            else if (ch == "5") install_epiphany();
            else if (ch == "6") install_midori();
            Logger::press_enter();
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // 1. Firefox & Chromium — 对应旧 Bash firefox_or_chromium
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::firefox_or_chromium() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"请从两个小可爱中选择一个\" --yes-button \"chromium\" --no-button \"Firefox\""
                          " --yesno 'I am Firefox, choose me. 我是火狐娘，选我啦！\n"
                          "妾身乃chrome娘的姐姐chromium娘，妾身和那些妖艳的货色不一样，\n"
                          "选择妾身就没错呢！请做出您的选择！' 15 50";
        auto r = Executor::passthrough(cmd);
        if (r.exit_code == 0) chromium_browser_menu(); // yes=chromium
        else if (r.exit_code == 1) firefox_or_firefoxesr(); // no=Firefox
        // exit_code 255 = ESC, do nothing
    }

    void BrowserManager::firefox_or_firefoxesr() {
        // 先问安装还是卸载
        std::string cmd = cfg_.tui_bin +
                          " --title \"FIREFOX 安装与卸载\" --yes-button \"install 安装\" --no-button \"remove 卸载\""
                          " --yesno \"Do you want to install or remove Firefox?\" 8 50";
        auto r = Executor::passthrough(cmd);
        bool do_remove = (r.exit_code == 1);

        // 再选版本
        cmd = cfg_.tui_bin +
              " --title \"请从两个小可爱中选择一个\" --yes-button \"Firefox\" --no-button \"ESR\""
              " --yesno 'I am Firefox,I have a younger sister called ESR.\n"
              "我是火狐娘，其实我还有个妹妹叫esr，您是选她还是选我?\n"
              "躲在姐姐背后的esr略带羞愤地说。请做出您的选择！' 12 53";
        r = Executor::passthrough(cmd);

        if (do_remove) {
            if (r.exit_code == 0) remove_firefox();
            else if (r.exit_code == 1) remove_firefox_esr();
        } else {
            if (r.exit_code == 0) install_firefox();
            else if (r.exit_code == 1) install_firefox_esr();
        }
    }

    void BrowserManager::chromium_browser_menu() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"CHROMIUM安装与卸载\" --yes-button \"install\" --no-button \"remove\""
                          " --yesno \"Do you want to install chromium or remove it?\" 8 50";
        auto r = Executor::passthrough(cmd);
        if (r.exit_code == 0) install_chromium();
        else if (r.exit_code == 1) remove_chromium();
    }

    void BrowserManager::microsoft_edge_menu() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"Do you want to install or remove edge?\""
                          " --yes-button \"install安装\" --no-button \"remove移除\""
                          " --yesno \"Microsoft Edge is a cross-platform web browser developed by Microsoft.\" 10 50";
        auto r = Executor::passthrough(cmd);
        if (r.exit_code == 0) install_edge();
        else if (r.exit_code == 1) remove_edge();
    }

    void BrowserManager::falkon_browser_menu() {
        std::string cmd = cfg_.tui_bin +
                          " --title \"FALKON安装与卸载\" --yes-button \"install\" --no-button \"remove\""
                          " --yesno \"Do you want to install falkon or remove it?\" 8 50";
        auto r = Executor::passthrough(cmd);
        if (r.exit_code == 0) install_falkon();
        else if (r.exit_code == 1) remove_falkon();
    }

    // ═══════════════════════════════════════════════════════════════
    // Firefox (匹配旧 Bash install_firefox_browser)
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::install_firefox() {
        Logger::step("Firefox");
        auto family = infer_family_from_config(cfg_.linux_distro);
        bool ok = false;

        switch (family) {
            case DistroFamily::Debian:
                ensure_firefox_ppa();
                ok = PackageManager::install("firefox", family);
                break;
            case DistroFamily::Arch:
                ok = PackageManager::install({"firefox", "firefox-i18n-zh-cn", "firefox-i18n-zh-tw"}, family);
                break;
            case DistroFamily::Gentoo:
                ok = PackageManager::install("www-client/firefox-bin", family);
                break;
            case DistroFamily::Suse:
                ok = PackageManager::install({"MozillaFirefox", "MozillaFirefox-translations-common"}, family);
                break;
            case DistroFamily::RedHat:
                ok = PackageManager::install("firefox-x11", family);
                break;
            default:
                ok = PackageManager::install("firefox", family);
        }
        if (!ok) return;
        create_no_sandbox_wrapper("Firefox", "firefox");
    }

    void BrowserManager::install_firefox_esr() {
        Logger::step("Firefox ESR");
        auto family = infer_family_from_config(cfg_.linux_distro);
        bool ok = false;

        switch (family) {
            case DistroFamily::Debian:
                ensure_firefox_ppa();
                ok = PackageManager::install("firefox-esr", family);
                if (ok) Executor::passthrough(cfg_.install_command + " ffmpeg ^firefox-esr-locale-zh || true");
                break;
            case DistroFamily::Arch:
                ok = PackageManager::install({"firefox-esr", "firefox-i18n-zh-cn"}, family);
                break;
            case DistroFamily::Gentoo:
                ok = PackageManager::install("www-client/firefox", family);
                break;
            case DistroFamily::Suse:
                ok = PackageManager::install({"MozillaFirefox-esr", "MozillaFirefox-esr-translations-common"}, family);
                break;
            default:
                ok = PackageManager::install("firefox-esr", family);
        }

        if (!ok) {
            Logger::warn("Firefox ESR 安装失败，尝试安装普通 Firefox...");
            install_firefox();
            return;
        }
        create_no_sandbox_wrapper("Firefox", "firefox-esr");
    }

    // ═══════════════════════════════════════════════════════════════
    // Chromium (匹配旧 Bash install_chromium_browser)
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::install_chromium() {
        Logger::step("Chromium");
        auto family = infer_family_from_config(cfg_.linux_distro);
        bool ok = false;

        if (family == DistroFamily::Debian) {
            ensure_chromium_ppa();
            if (cfg_.linux_distro.find("ubuntu") != std::string::npos
                || cfg_.linux_distro.find("Ubuntu") != std::string::npos) {
                ok = PackageManager::install("chromium-browser", family);
            } else {
                ok = PackageManager::install("chromium", family);
            }
        } else if (family == DistroFamily::Gentoo) {
            ok = PackageManager::install("www-client/chromium", family);
        } else if (family == DistroFamily::Suse) {
            ok = PackageManager::install({"chromium", "chromium-plugin-widevinecdm", "chromium-ffmpeg-extra"}, family);
        } else if (family == DistroFamily::RedHat) {
            ok = PackageManager::install({"chromium", "fedora-chromium-config"}, family);
        } else {
            ok = PackageManager::install("chromium", family);
        }

        if (!ok)
            ok = PackageManager::install("chromium-browser", family);
        if (!ok) return;

        create_no_sandbox_wrapper("Chromium", "chromium");
    }

    // ═══════════════════════════════════════════════════════════════
    // Microsoft Edge (匹配旧 Bash install_microsoft_edge + 跨发行版)
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::install_edge() {
        Logger::step("Microsoft Edge");
        auto family = infer_family_from_config(cfg_.linux_distro);
        bool ok = false;

        switch (family) {
            case DistroFamily::Debian:
                ensure_edge_repo();
                ok = PackageManager::install("microsoft-edge-dev", family);
                break;
            case DistroFamily::RedHat:
                Executor::passthrough("rpm --import https://packages.microsoft.com/keys/microsoft.asc");
                Executor::passthrough("dnf config-manager --add-repo "
                    "https://packages.microsoft.com/yumrepos/edge");
                ok = Executor::passthrough("dnf install -y microsoft-edge-dev").ok();
                break;
            case DistroFamily::Suse:
                Executor::passthrough("rpm --import https://packages.microsoft.com/keys/microsoft.asc");
                Executor::passthrough("zypper ar https://packages.microsoft.com/yumrepos/edge microsoft-edge-dev");
                ok = Executor::passthrough("zypper refresh && zypper install -y microsoft-edge-dev").ok();
                break;
            case DistroFamily::Arch:
                ok = PackageManager::install("microsoft-edge-dev", family);
                break;
            default:
                ok = PackageManager::install("microsoft-edge-dev", family);
        }
        if (!ok) return;

        create_no_sandbox_wrapper("Edge", "microsoft-edge-dev");
        // 将 Edge 的桌面快捷方式也复制到桌面
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        Executor::shell("for f in /usr/share/applications/microsoft-edge*.desktop; do "
                        "  [ -f \"$f\" ] && cp -f \"$f\" " + home + "/Desktop/ 2>/dev/null; "
                        "done; true");
    }

    void BrowserManager::install_falkon() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step("Falkon");
        if (!PackageManager::install("falkon", family)) return;
        create_no_sandbox_wrapper("Falkon", "falkon");
    }

    void BrowserManager::install_vivaldi() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step("Vivaldi");
        std::string pkg = (family == DistroFamily::Arch) ? "vivaldi" : "vivaldi-stable";
        if (!PackageManager::install(pkg, family)) return;
        create_no_sandbox_wrapper("Vivaldi", "vivaldi-stable");
    }

    void BrowserManager::install_epiphany() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step("Epiphany");
        PackageManager::install("epiphany-browser", family);
    }

    void BrowserManager::install_midori() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step("Midori");
        PackageManager::install("midori", family);
    }

    // ═══════════════════════════════════════════════════════════════
    // Remove 浏览器 (匹配旧 Bash remove 逻辑)
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::remove_chromium() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --yesno \"确认卸载 Chromium？\" 8 50");
        if (r.exit_code != 0) return;
        if (family == DistroFamily::Debian)
            Executor::passthrough("apt-mark unhold chromium-browser chromium-browser-l10n "
                "chromium-codecs-ffmpeg-extra 2>/dev/null || true");
        for (const auto &pkg: {"chromium", "chromium-l10n", "chromium-browser", "chromium-browser-l10n"})
            Executor::passthrough(cfg_.remove_command + " " + pkg + " 2>/dev/null || true");
        Executor::passthrough("rm -vf /usr/local/bin/chromium--no-sandbox "
            "/usr/local/share/applications/chromium-browser-no-sandbox.desktop "
            "${HOME}/Desktop/chromium-browser-no-sandbox.desktop 2>/dev/null || true");
    }

    void BrowserManager::remove_edge() {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --yesno \"确认卸载 Microsoft Edge？\" 8 50");
        if (r.exit_code != 0) return;
        auto family = infer_family_from_config(cfg_.linux_distro);
        Executor::passthrough(cfg_.remove_command + " microsoft-edge-dev || true");
        switch (family) {
            case DistroFamily::Debian:
                Executor::passthrough("rm -vf /etc/apt/sources.list.d/microsoft-edge-dev.list 2>/dev/null || true");
                Executor::passthrough("apt update 2>/dev/null || true");
                break;
            case DistroFamily::RedHat:
                Executor::passthrough("rm -vf /etc/yum.repos.d/microsoft-edge-dev.repo 2>/dev/null || true");
                break;
            case DistroFamily::Suse:
                Executor::passthrough("zypper removerepo microsoft-edge-dev 2>/dev/null || true");
                break;
            default: break;
        }
        // 清理 no-sandbox 快捷方式 + 桌面副本
        Executor::passthrough("rm -vf /usr/local/bin/microsoft-edge-dev--no-sandbox "
            "/usr/local/share/applications/microsoft-edge-dev-no-sandbox.desktop "
            "${HOME}/Desktop/microsoft-edge*.desktop 2>/dev/null || true");
    }

    void BrowserManager::remove_falkon() {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --yesno \"确认卸载 Falkon？\" 8 50");
        if (r.exit_code != 0) return;
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::remove("falkon", family);
        Executor::passthrough("rm -vf /usr/local/bin/falkon--no-sandbox "
            "/usr/local/share/applications/falkon-no-sandbox.desktop 2>/dev/null || true");
    }

    void BrowserManager::remove_firefox() {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --yesno \"确认卸载 Firefox？\" 8 50");
        if (r.exit_code != 0) return;
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Debian) {
            Executor::passthrough("apt purge -y firefox ^firefox-locale 2>/dev/null || true");
            // 如果 Firefox ESR 也已卸载，清理 PPA apt pinning
            bool has_esr = Executor::shell("dpkg -l firefox-esr 2>/dev/null | grep -q '^ii'").ok();
            if (!has_esr)
                Executor::passthrough("rm -vf /etc/apt/preferences.d/mozilla-firefox 2>/dev/null || true");
        } else {
            Executor::passthrough(cfg_.remove_command + " firefox 2>/dev/null || true");
        }
        // 只清理 Firefox (非 ESR) 的 no-sandbox 快捷方式
        Executor::passthrough("rm -vf /usr/local/bin/firefox--no-sandbox "
            "/usr/local/share/applications/firefox-no-sandbox.desktop "
            "${HOME}/Desktop/firefox-no-sandbox.desktop 2>/dev/null || true");
    }

    void BrowserManager::remove_firefox_esr() {
        auto r = Executor::passthrough(cfg_.tui_bin +
                                       " --yesno \"确认卸载 Firefox ESR？\" 8 50");
        if (r.exit_code != 0) return;
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Debian) {
            Executor::passthrough("apt purge -y firefox-esr ^firefox-esr-locale 2>/dev/null || true");
            // 如果同时卸载了两个版本，清理 PPA 和 apt pinning
            bool has_regular = Executor::shell("dpkg -l firefox 2>/dev/null | grep -q '^ii'").ok();
            if (!has_regular)
                Executor::passthrough("rm -vf /etc/apt/preferences.d/mozilla-firefox 2>/dev/null || true");
        } else {
            Executor::passthrough(cfg_.remove_command + " firefox-esr 2>/dev/null || true");
        }
        // 只清理 Firefox ESR 的 no-sandbox 快捷方式
        Executor::passthrough("rm -vf /usr/local/bin/firefox-esr--no-sandbox "
            "/usr/local/share/applications/firefox-esr-no-sandbox.desktop "
            "${HOME}/Desktop/firefox-esr-no-sandbox.desktop 2>/dev/null || true");
    }

    // ═══════════════════════════════════════════════════════════════
    // 辅助
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::create_no_sandbox_wrapper(const std::string &name, const std::string &bin_name) {
        bool is_root = cfg_.is_root || cfg_.is_termux;
        if (!is_root && cfg_.linux_distro != "unknown") {
            auto id_result = Executor::shell("id -u");
            is_root = id_result.ok() && id_result.stdout_data.find('0') == 0;
        }
        if (!is_root) return;

        Logger::step(_("browser.no_sandbox_wrapper"));

        std::string wrapper_path = "/usr/local/bin/" + bin_name + "--no-sandbox";
        std::string bin_search = (bin_name == "chromium") ? "chromium-browser" : bin_name;

        std::ofstream wrapper(wrapper_path);
        wrapper << "#!/bin/bash\n"
                << "# Auto-generated by tmoe-linux — run " << name << " without sandbox\n\n"
                << "BIN=\"\"\n"
                << "for CANDIDATE in " << bin_name << " " << bin_search << "; do\n"
                << "    if command -v \"$CANDIDATE\" >/dev/null 2>&1; then\n"
                << "        BIN=\"$CANDIDATE\"\n"
                << "        break\n"
                << "    fi\n"
                << "done\n\n"
                << "if [ -z \"$BIN\" ]; then\n"
                << "    echo \"" << name << " not found\" >&2\n"
                << "    exit 1\n"
                << "fi\n\n"
                << "[ \"$(id -u)\" -eq 0 ] && exec \"$BIN\" --no-sandbox \"$@\"\n"
                << "exec \"$BIN\" \"$@\"\n";
        wrapper.close();

        Executor::shell("chmod 755 " + wrapper_path);
        Logger::ok(_("browser.wrapper_created") + ": " + wrapper_path);

        // 确定图标名 (匹配 /usr/share/icons 下的实际 icon 文件名)
        std::string icon = bin_name;
        if (bin_name == "firefox") icon = "firefox";
        else if (bin_name == "firefox-esr") icon = "firefox-esr";
        else if (bin_name == "chromium") icon = "chromium";
        else if (bin_name == "microsoft-edge-dev") icon = "microsoft-edge-dev";

        Logger::step(_("browser.no_sandbox_desktop"));
        std::string desktop_dir = "/usr/local/share/applications";
        Executor::shell("mkdir -p " + desktop_dir + " 2>/dev/null");
        std::string desktop_path = desktop_dir + "/" + bin_name + "-no-sandbox.desktop";
        std::ofstream desktop(desktop_path);
        desktop << "[Desktop Entry]\n"
                << "Name=" << name << " (No Sandbox)\n"
                << "Exec=" << wrapper_path << "\n"
                << "Icon=" << icon << "\n"
                << "Type=Application\n"
                << "Categories=Network;WebBrowser;\n"
                << "Terminal=false\n";
        desktop.close();
        Executor::shell("chmod 644 " + desktop_path + " 2>/dev/null");

        // 复制到所有用户的桌面 (proot 下可能是 root 或普通用户)
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string sudo_user = std::getenv("SUDO_USER") ? std::getenv("SUDO_USER") : "";
        Executor::shell("mkdir -p " + home + "/Desktop 2>/dev/null; "
                        "cp -f " + desktop_path + " " + home + "/Desktop/ 2>/dev/null || true");
        if (!sudo_user.empty() && sudo_user != "root") {
            Executor::shell("cp -f " + desktop_path + " /home/" + sudo_user + "/Desktop/ 2>/dev/null || true");
        }
    }

    void BrowserManager::ensure_firefox_ppa() {
        bool needs_update = false;

        // 1. 添加 PPA 源
        bool has_ppa = false;
        std::error_code ec;
        if (fs::exists("/etc/apt/sources.list.d", ec)) {
            for (const auto &entry: fs::directory_iterator("/etc/apt/sources.list.d")) {
                if (entry.path().filename().string().find("mozillateam") != std::string::npos) {
                    has_ppa = true;
                    break;
                }
            }
        }
        if (!has_ppa) {
            Logger::step(_("browser.firefox_ppa"));
            Executor::passthrough("add-apt-repository -y ppa:mozillateam/ppa 2>/dev/null || "
                "add-apt-repository -y ppa:mozillateam/ppa 2>/dev/null || true");
            needs_update = true;
        }

        // 2. 设置 apt pinning — 阻止 snap 过渡包，优先用 PPA 的 .deb
        std::string pref_file = "/etc/apt/preferences.d/mozilla-firefox";
        if (!fs::exists(pref_file)) {
            std::ofstream prefs(pref_file);
            if (prefs.is_open()) {
                prefs << "Package: *\n"
                        << "Pin: release o=LP-PPA-mozillateam\n"
                        << "Pin-Priority: 1001\n"
                        << "\n"
                        << "Package: firefox\n"
                        << "Pin: version 1:1snap1-0ubuntu2\n"
                        << "Pin-Priority: -1\n";
                prefs.close();
                Logger::info("已设置 Firefox apt pinning 优先级");
                needs_update = true;
            }
        }

        // 3. 如果新增了 PPA 或 pinning，刷新 apt 缓存
        if (needs_update) {
            Executor::passthrough("apt update 2>/dev/null || true");
        }
    }

    void BrowserManager::ensure_chromium_ppa() {
        std::error_code ec;
        if (fs::exists("/etc/apt/sources.list.d", ec)) {
            for (const auto &entry: fs::directory_iterator("/etc/apt/sources.list.d")) {
                if (entry.path().filename().string().find("saiarcot895") != std::string::npos)
                    return;
            }
        }

        Logger::step(_("browser.chromium_ppa"));
        Executor::passthrough(
            "add-apt-repository -y ppa:saiarcot895/chromium-dev || "
            "add-apt-repository -y ppa:saiarcot895/chromium-dev || true"
        );
    }

    void BrowserManager::ensure_edge_repo() {
        // 源文件已存在就跳过
        if (fs::exists("/etc/apt/sources.list.d/microsoft-edge.list") ||
            fs::exists("/etc/apt/sources.list.d/microsoft-edge-dev.list"))
            return;

        Logger::step(_("browser.edge_repo") + ": GPG key");
        Executor::passthrough(
            "curl -fsSL https://packages.microsoft.com/keys/microsoft.asc | "
            "gpg --dearmor -o /etc/apt/trusted.gpg.d/microsoft.gpg"
        );

        Logger::step(_("browser.edge_repo") + ": sources.list");
        fs::create_directories("/etc/apt/sources.list.d");
        std::ofstream ofs("/etc/apt/sources.list.d/microsoft-edge.list");
        ofs << "deb [arch=amd64] https://packages.microsoft.com/repos/edge stable main\n";
        ofs.close();

        Logger::step(_("browser.edge_repo") + ": apt update");
        Executor::passthrough("apt update");
    }
} // namespace tmoe::domain
