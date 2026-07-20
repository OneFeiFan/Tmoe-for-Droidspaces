#include "domain/apps/browser.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "core/command_builder.hpp"
#include "domain/system/package_manager.h"
#include <fstream>

#include "core/system_helper.h"
#include "core/str_utils.h"

namespace tmoe::domain {
    BrowserManager::BrowserManager(const TmoeConfig &cfg) : cfg_(cfg) {
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
                if (ok) PackageManager::install({"ffmpeg", "^firefox-esr-locale-zh"}, family);
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
            Logger::warn(_("browser.firefox_esr_failed_fallback"));
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
            if (contains(cfg_.linux_distro, "ubuntu")
                || contains(cfg_.linux_distro, "Ubuntu")) {
                ok = PackageManager::install({
                                                 "chromium-browser", "chromium-browser-l10n",
                                                 "chromium-chromedriver", "chromium-shell"
                                             }, family);
            } else {
                ok = PackageManager::install({
                                                 "chromium", "chromium-l10n",
                                                 "chromium-driver", "chromium-shell"
                                             }, family);
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

        // 移除 Debian 系强制锁定 DuckDuckGo 搜索引擎的 policy 文件
        Executor::shell(
            "sudo rm -rf /etc/chromium/policies/managed/ /etc/chromium-browser/policies/managed/ 2>/dev/null || true");
        Logger::info(_("browser.chromium_policy_cleaned"));

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
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rpm").add_flag("--import")
                    .add_arg("https://packages.microsoft.com/keys/microsoft.asc").build_string());
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("dnf").add_arg("config-manager").add_flag("--add-repo")
                    .add_arg("https://packages.microsoft.com/yumrepos/edge").build_string());
                ok = PackageManager::install("microsoft-edge-dev", family);
                break;
            case DistroFamily::Suse:
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rpm").add_flag("--import")
                    .add_arg("https://packages.microsoft.com/keys/microsoft.asc").build_string());
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("zypper").add_flag("ar")
                    .add_arg("https://packages.microsoft.com/yumrepos/edge")
                    .add_arg("microsoft-edge-dev").build_string());
                Executor::passthrough(CommandBuilder("sudo").add_arg("zypper").add_arg("refresh").build_string());
                ok = PackageManager::install("microsoft-edge-dev", family);
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
        if (family == DistroFamily::Debian)
            Executor::passthrough(
                CommandBuilder("sudo").add_arg("apt-mark").add_flag("unhold")
                .add_arg("chromium-browser").add_arg("chromium-browser-l10n")
                .add_arg("chromium-codecs-ffmpeg-extra")
                .add_raw("2>/dev/null || true").build_string());
        Executor::passthrough("sudo add-apt-repository -ry ppa:xtradeb/apps 2>/dev/null || true");
        for (const auto &pkg: {"chromium", "chromium-l10n", "chromium-browser", "chromium-browser-l10n"})
            PackageManager::remove(pkg, family);
        Executor::passthrough(
            CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
            .add_arg("/usr/local/bin/chromium--no-sandbox")
            .add_arg("/usr/local/share/applications/chromium-browser-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
        Executor::passthrough(
            CommandBuilder("rm").add_flag("-vf")
            .add_arg("${HOME}/Desktop/chromium-browser-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
    }

    void BrowserManager::remove_edge() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::remove("microsoft-edge-dev", family);
        switch (family) {
            case DistroFamily::Debian:
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
                    .add_arg("/etc/apt/sources.list.d/microsoft-edge.list")
                    .add_arg("/etc/apt/sources.list.d/microsoft-edge-dev.list")
                    .add_arg("/etc/apt/trusted.gpg.d/microsoft.gpg")
                    .add_raw("2>/dev/null || true").build_string());
                PackageManager::update(DistroFamily::Debian);
                break;
            case DistroFamily::RedHat:
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
                    .add_arg("/etc/yum.repos.d/microsoft-edge-dev.repo")
                    .add_raw("2>/dev/null || true").build_string());
                break;
            case DistroFamily::Suse:
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("zypper").add_flag("removerepo")
                    .add_arg("microsoft-edge-dev")
                    .add_raw("2>/dev/null || true").build_string());
                break;
            default: break;
        }
        // 清理 no-sandbox 快捷方式 + 桌面副本
        Executor::passthrough(
            CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
            .add_arg("/usr/local/bin/microsoft-edge-dev--no-sandbox")
            .add_arg("/usr/local/share/applications/microsoft-edge-dev-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
        Executor::passthrough(
            CommandBuilder("rm").add_flag("-vf")
            .add_arg("${HOME}/Desktop/microsoft-edge*.desktop")
            .add_raw("2>/dev/null || true").build_string());
    }

    void BrowserManager::remove_falkon() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::remove("falkon", family);
        Executor::passthrough(
            CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
            .add_arg("/usr/local/bin/falkon--no-sandbox")
            .add_arg("/usr/local/share/applications/falkon-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
    }

    void BrowserManager::remove_firefox() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Debian) {
            Executor::passthrough(
                CommandBuilder("sudo").add_arg("apt").add_flag("purge").add_flag("-y")
                .add_arg("firefox").add_arg("^firefox-locale")
                .add_raw("2>/dev/null || true").build_string());
            bool has_esr = Executor::shell("dpkg -l firefox-esr 2>/dev/null | grep -q '^ii'").ok();
            if (!has_esr) {
                // 两个版本都卸了 → 清理 PPA 源和 apt pinning
                Executor::passthrough("sudo add-apt-repository -ry ppa:mozillateam/ppa 2>/dev/null || true");
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
                    .add_arg("/etc/apt/preferences.d/mozilla-firefox")
                    .add_raw("2>/dev/null || true").build_string());
            }
        } else {
            PackageManager::remove("firefox", family);
        }
        Executor::passthrough(
            CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
            .add_arg("/usr/local/bin/firefox--no-sandbox")
            .add_arg("/usr/local/share/applications/firefox-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
        Executor::passthrough(
            CommandBuilder("rm").add_flag("-vf")
            .add_arg("${HOME}/Desktop/firefox-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
    }

    void BrowserManager::remove_firefox_esr() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Debian) {
            Executor::passthrough(
                CommandBuilder("sudo").add_arg("apt").add_flag("purge").add_flag("-y")
                .add_arg("firefox-esr").add_arg("^firefox-esr-locale")
                .add_raw("2>/dev/null || true").build_string());
            bool has_regular = Executor::shell("dpkg -l firefox 2>/dev/null | grep -q '^ii'").ok();
            if (!has_regular) {
                Executor::passthrough("sudo add-apt-repository -ry ppa:mozillateam/ppa 2>/dev/null || true");
                Executor::passthrough(
                    CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
                    .add_arg("/etc/apt/preferences.d/mozilla-firefox")
                    .add_raw("2>/dev/null || true").build_string());
            }
        } else {
            PackageManager::remove("firefox-esr", family);
        }
        Executor::passthrough(
            CommandBuilder("sudo").add_arg("rm").add_flag("-vf")
            .add_arg("/usr/local/bin/firefox-esr--no-sandbox")
            .add_arg("/usr/local/share/applications/firefox-esr-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
        Executor::passthrough(
            CommandBuilder("rm").add_flag("-vf")
            .add_arg("${HOME}/Desktop/firefox-esr-no-sandbox.desktop")
            .add_raw("2>/dev/null || true").build_string());
    }

    // ═══════════════════════════════════════════════════════════════
    // 辅助
    // ═══════════════════════════════════════════════════════════════
    void BrowserManager::create_no_sandbox_wrapper(const std::string &name, const std::string &bin_name) {
        Logger::step(_("browser.no_sandbox_wrapper"));

        std::string wrapper_path = "/usr/local/bin/" + bin_name + "--no-sandbox";
        std::string bin_search = (bin_name == "chromium") ? "chromium-browser" : bin_name;

        std::string wrapper_content;
        wrapper_content = "#!/bin/bash\n"
                          "# Auto-generated by tmoe-linux — run " + name + " without sandbox\n\n"
                          "BIN=\"\"\n"
                          "for CANDIDATE in " + bin_name + " " + bin_search + "; do\n"
                          "    if command -v \"$CANDIDATE\" >/dev/null 2>&1; then\n"
                          "        BIN=\"$CANDIDATE\"\n"
                          "        break\n"
                          "    fi\n"
                          "done\n\n"
                          "if [ -z \"$BIN\" ]; then\n"
                          "    echo \"" + name + " not found\" >&2\n"
                          "    exit 1\n"
                          "fi\n\n"
                          "[ \"$(id -u)\" -eq 0 ] && exec \"$BIN\" --no-sandbox \"$@\"\n"
                          "exec \"$BIN\" \"$@\"\n";
        SystemHelper::write_file(wrapper_path, wrapper_content);

        CommandBuilder("sudo").add_arg("chmod").add_arg("755").add_arg(wrapper_path).execute();
        Logger::ok(_("browser.wrapper_created") + ": " + wrapper_path);

        std::string icon = bin_name;
        if (bin_name == "firefox") icon = "firefox";
        else if (bin_name == "firefox-esr") icon = "firefox-esr";
        else if (bin_name == "chromium") icon = "chromium";
        else if (bin_name == "microsoft-edge-dev") icon = "microsoft-edge-dev";

        Logger::step(_("browser.no_sandbox_desktop"));
        std::string desktop_dir = "/usr/local/share/applications";
        Executor::shell(
            CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg(desktop_dir)
            .add_raw("2>/dev/null").build_string());
        std::string desktop_path = desktop_dir + "/" + bin_name + "-no-sandbox.desktop";
        std::string desktop_content =
                "[Desktop Entry]\n"
                "Name=" + name + " (No Sandbox)\n"
                "Exec=" + wrapper_path + "\n"
                "Icon=" + icon + "\n"
                "Type=Application\n"
                "Categories=Network;WebBrowser;\n"
                "Terminal=false\n";
        SystemHelper::write_file(desktop_path, desktop_content);

        Executor::shell(
            CommandBuilder("sudo").add_arg("chmod").add_arg("644").add_arg(desktop_path)
            .add_raw("2>/dev/null").build_string());

        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        Executor::shell(
            CommandBuilder("mkdir").add_flag("-p").add_arg(home + "/Desktop")
            .add_raw("2>/dev/null").build_string());
        Executor::shell(
            CommandBuilder("cp").add_flag("-f").add_arg(desktop_path).add_arg(home + "/Desktop/")
            .add_raw("2>/dev/null || true").build_string());

        std::string sudo_user = std::getenv("SUDO_USER") ? std::getenv("SUDO_USER") : "";
        if (!sudo_user.empty() && sudo_user != "root") {
            Executor::shell(
                CommandBuilder("sudo").add_arg("cp").add_flag("-f").add_arg(desktop_path)
                .add_arg("/home/" + sudo_user + "/Desktop/")
                .add_raw("2>/dev/null || true").build_string());
        }
    }

    void BrowserManager::ensure_firefox_ppa() {
        bool needs_update = false;

        // 1. 添加 PPA 源
        bool has_ppa = false;
        std::error_code ec;
        if (fs::exists("/etc/apt/sources.list.d", ec)) {
            for (const auto &entry: fs::directory_iterator("/etc/apt/sources.list.d")) {
                if (contains(entry.path().filename().string(), "mozillateam")) {
                    has_ppa = true;
                    break;
                }
            }
        }
        if (!has_ppa) {
            Logger::step(_("browser.firefox_ppa"));
            Executor::passthrough("sudo add-apt-repository -y ppa:mozillateam/ppa 2>/dev/null || "
                "sudo add-apt-repository -y ppa:mozillateam/ppa 2>/dev/null || true");
            needs_update = true;
        }

        // 2. 设置 apt pinning — 阻止 snap 过渡包，优先用 PPA 的 .deb
        std::string pref_file = "/etc/apt/preferences.d/mozilla-firefox";
        if (!fs::exists(pref_file)) {
            SystemHelper::write_file(pref_file,
                                     "Package: *\n"
                                     "Pin: release o=LP-PPA-mozillateam\n"
                                     "Pin-Priority: 1001\n"
                                     "\n"
                                     "Package: firefox\n"
                                     "Pin: version 1:1snap1-0ubuntu2\n"
                                     "Pin-Priority: -1\n");
            Logger::info(_("browser.firefox_apt_pinning_set"));
            needs_update = true;
        }

        // 3. 如果新增了 PPA 或 pinning，刷新 apt 缓存
        if (needs_update) {
            PackageManager::update(DistroFamily::Debian);
        }
    }

    void BrowserManager::ensure_chromium_ppa() {
        std::error_code ec;
        if (fs::exists("/etc/apt/sources.list.d", ec)) {
            for (const auto &entry: fs::directory_iterator("/etc/apt/sources.list.d")) {
                if (contains(entry.path().filename().string(), "xtradeb"))
                    return;
            }
        }

        Logger::step(_("browser.chromium_ppa"));
        Executor::passthrough(
            "sudo add-apt-repository -y ppa:xtradeb/apps || "
            "sudo add-apt-repository -y ppa:xtradeb/apps || true"
        );
    }

    void BrowserManager::ensure_edge_repo() {
        if (fs::exists("/etc/apt/sources.list.d/microsoft-edge.list") ||
            fs::exists("/etc/apt/sources.list.d/microsoft-edge-dev.list"))
            return;

        // Microsoft Edge 仅提供 amd64 包
        auto arch_result = Executor::shell("dpkg --print-architecture 2>/dev/null");
        std::string arch = arch_result.stdout_data;
        trim_newline(arch);
        if (arch != "amd64") {
            Logger::warn("Microsoft Edge only supports amd64, skipped (current: " + arch + ")");
            return;
        }

        Logger::step(_("browser.edge_repo") + ": GPG key");
        Executor::passthrough(
            "curl -fSL --progress-bar https://packages.microsoft.com/keys/microsoft.asc | "
            "sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/microsoft.gpg 2>/dev/null || true");

        Logger::step(_("browser.edge_repo") + ": sources.list");
        SystemHelper::write_file("/etc/apt/sources.list.d/microsoft-edge.list",
                                 "deb [arch=amd64] https://packages.microsoft.com/repos/edge stable main\n");

        Logger::step(_("browser.edge_repo") + ": apt update");
        PackageManager::update(DistroFamily::Debian);
    }
} // namespace tmoe::domain
