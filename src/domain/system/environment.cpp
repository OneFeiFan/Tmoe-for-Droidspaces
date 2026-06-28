#include "domain/system/environment.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"

namespace tmoe::domain {
    bool Environment::initialize() {
        Logger::step(_("env.initializing"));
        cfg_.ensure_dirs();
        // TODO: 设置临时文件、清理残留状态
        return true;
    }

    bool Environment::check_dependencies() {
        if (cfg_.is_termux) {
            // Android 依赖由 TermuxManager 接管处理
            return true;
        }

        Logger::step(_("env.checking_deps"));

        // =================================================================
        // 阶段 1: 包管理器健康检查与自愈 (前置执行，防止后续 install 崩溃)
        // =================================================================
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu") {
            auto audit = Executor::shell("dpkg --audit 2>/dev/null");
            if (!audit.stdout_data.empty() || audit.exit_code != 0) {
                Logger::warn(_("env.dpkg_abnormal"));
                bool has_issue = false;

                // 检查是否需要在 dpkg --configure -a
                auto pending = Executor::shell("ls /var/lib/dpkg/updates/tmp.* 2>/dev/null | head -1");
                if (!pending.stdout_data.empty()) {
                    has_issue = true;
                    Logger::info(_("env.dpkg_incomplete_config"));
                }

                // 检查锁文件
                if (fs::exists("/var/lib/dpkg/lock") || fs::exists("/var/lib/dpkg/lock-frontend")) {
                    auto lock_owner = Executor::shell("fuser /var/lib/dpkg/lock 2>/dev/null");
                    if (lock_owner.stdout_data.empty()) {
                        has_issue = true;
                        Logger::info(_("env.dpkg_stale_lock"));
                    } else {
                        Logger::info(_("env.dpkg_process_running"));
                    }
                }

                // 检查破损依赖
                auto broken = Executor::shell("apt-get check 2>&1");
                if (!broken.ok()) {
                    has_issue = true;
                    Logger::info(_("env.dpkg_broken_deps"));
                }

                if (has_issue && Logger::confirm(_("env.confirm_repair_dpkg"))) {
                    Logger::step(_("env.repairing_dpkg"));
                    // 先清理残留锁文件（无进程持有时）
                    Executor::shell("sudo fuser /var/lib/dpkg/lock 2>/dev/null || "
                        "rm -f /var/lib/dpkg/lock /var/lib/dpkg/lock-frontend 2>/dev/null || true");
                    // 修复 dpkg 配置
                    Executor::passthrough("sudo dpkg --configure -a 2>&1 || true");
                    // 修复破损依赖
                    Executor::passthrough("sudo apt --fix-broken install -y 2>&1 || true");
                    Logger::ok(_("env.dpkg_repair_ok"));
                } else if (!has_issue) {
                    Logger::info(_("env.dpkg_skip"));
                }
            }
        }

        // =================================================================
        // 阶段 2: 核心依赖收集
        // =================================================================
        std::string missing_pkgs = "";

        // 1. 检查 sudo (Alpine 除外)
        if (!Executor::has("sudo") && cfg_.linux_distro != "alpine") {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-admin/sudo" : " sudo";
        }

        // 2. 检查 TUI 核心组件
        if (!Executor::has("dialog") && !Executor::has("whiptail")) {
            if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
                missing_pkgs += (cfg_.linux_distro == "gentoo") ? " dev-util/dialog" : " dialog";
            } else if (cfg_.linux_distro == "openwrt") {
                missing_pkgs += " dialog whiptail";
            } else {
                missing_pkgs += " dialog";
            }
        }

        // 3. 检查网络工具 (解耦 curl 和 wget，由于核心 API 强依赖 curl，必须单独核验)
        if (!Executor::has("curl")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " net-misc/curl" : " curl";
        }
        if (!Executor::has("wget")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " net-misc/wget" : " wget";
        }

        // 4. 检查扩展工具 (下载/解压/加密/Git/JSON解析)
        if (!Executor::has("aria2c")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-misc/aria2" : " aria2";
        }
        if (!Executor::has("zstd")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-arch/zstd" : " zstd";
        }
        if (!Executor::has("gpg")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-crypt/gnupg" : " gnupg";
        }
        if (!Executor::has("git")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " dev-vcs/git" : " git";
        }
        if (!Executor::has("jq")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " app-misc/jq" : " jq";
        }

        if (!Executor::has("pv")) {
            missing_pkgs += (cfg_.linux_distro == "gentoo") ? " sys-apps/pv" : " pv";
        }

        // =================================================================
        // 阶段 3: 执行统一安装
        // =================================================================
        if (!missing_pkgs.empty()) {
            Logger::warn(_("env.missing_deps") + ": " + missing_pkgs);
            Logger::info(_("env.install_cmd") + ": " + cfg_.install_command + missing_pkgs);

            // 刷新源并安装
            Executor::passthrough(cfg_.update_command);
            if (Executor::passthrough(cfg_.install_command + missing_pkgs).ok()) {
                Logger::ok(_("env.deps_ok"));
            } else {
                Logger::error(_("env.deps_failed"));
                // 阻断前给出确切的排查提示
                Logger::info(_("env.check_network"));
                return false;
            }
        } else {
            Logger::ok(_("env.deps_passed"));
        }

        // =================================================================
        // 阶段 4: 基础设施配置 (Locale / 字体)
        // =================================================================
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu") {
            if (!Executor::has("locale-gen")) {
                Logger::step(_("env.installing_locales"));
                Executor::passthrough(cfg_.install_command + " locales 2>/dev/null || true");
            }
        } else if (cfg_.linux_distro == "alpine") {
            if (Executor::shell("apk info -e musl-locales 2>/dev/null").exit_code != 0) {
                Logger::step(_("env.installing_musl_locales"));
                Executor::passthrough(cfg_.install_command + " musl-locales 2>/dev/null || true");
            }
        }
        return true;
    }

    bool Environment::set_locale(std::string_view loc) {
        std::string lang(loc);
        // 剥离 .UTF-8 后缀
        size_t dot = lang.find('.');
        if (dot != std::string::npos) {
            lang = lang.substr(0, dot);
        }

        std::string locale_str = lang + ".UTF-8";
        bool locale_ok = is_locale_generated(lang);

        if (!locale_ok) {
            Logger::step(_f("env.locale_generating", locale_str));

            if (generate_locale_auto(lang)) {
                locale_ok = true;
                Logger::ok(_f("env.locale_generated", locale_str));
            } else {
                Logger::warn(_f("env.locale_gen_failed", locale_str));

                // Debian 系: 提供交互式 dpkg-reconfigure locales 回退
                if (cfg_.linux_distro == "debian") {
                    std::string prompt = cfg_.tui_bin +
                                         " --title \"" + _("locale.config_title") + "\""
                                         " --yesno \"" + _("locale.gen_failed_yesno") + "\" 0 0";
                    auto choice = Executor::passthrough(prompt);
                    if (choice.exit_code == 0) {
                        generate_locale_debian_interactive();
                        if (is_locale_generated(lang)) {
                            locale_ok = true;
                            Logger::ok(_f("env.locale_generated", locale_str));
                        }
                    }
                }

                // 仍未成功 → 给出手动配置指引
                if (!locale_ok) {
                    Logger::warn(_("env.locale_incomplete"));
                    if (cfg_.linux_distro == "debian") {
                        Logger::info("  sudo dpkg-reconfigure locales");
                        Logger::info("  or: sudo locale-gen " + locale_str);
                    } else if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
                        Logger::info("  Edit /etc/locale.gen and uncomment " + locale_str);
                        Logger::info("  Then run: sudo locale-gen");
                    } else if (cfg_.linux_distro == "alpine") {
                        Logger::info("  sudo apk add musl-locales");
                    } else {
                        Logger::info("  Please ensure the system has generated " + locale_str + " locale");
                    }
                }
            }
        }

        // 字体覆盖检测与安装
        if (!has_font_coverage(lang)) {
            Logger::warn(_f("env.no_font_coverage", std::string(lang)));
            if (install_font_packages()) {
                Logger::ok(_("env.font_install_ok"));
                refresh_font_cache();
            } else {
                Logger::warn(_("env.font_install_failed"));
                if (needs_cjk(lang)) {
                    Logger::info(_("env.manual_cjk_fonts"));
                } else if (needs_cyrillic(lang)) {
                    Logger::info(_("env.manual_cyrillic_fonts"));
                }
            }
        }

        // 设置环境变量
#ifdef _WIN32
        _putenv_s("LANG", locale_str.c_str());
        _putenv_s("LC_ALL", locale_str.c_str());
        _putenv_s("LANGUAGE", locale_str.c_str());
#else
        ::setenv("LANG", locale_str.c_str(), 1);
        ::setenv("LC_ALL", locale_str.c_str(), 1);
        ::setenv("LANGUAGE", locale_str.c_str(), 1);
#endif

        Logger::debug("LANG=" + locale_str);
        return true;
    }

    bool Environment::apply_system_locale(std::string_view loc) {
        std::string lang(loc);
        size_t dot = lang.find('.');
        if (dot != std::string::npos) lang = lang.substr(0, dot);

        std::string locale_str = lang + ".UTF-8";

        // 方法1: localectl (systemd, 覆盖主流发行版)
        if (Executor::has("localectl")) {
            Logger::step(_("env.locale_persisting"));
            auto r = Executor::passthrough("localectl set-locale LANG=" + locale_str);
            if (r.ok()) {
                Logger::ok(_f("env.locale_persist_ok", locale_str));
                return true;
            }
            Logger::warn(_("env.localectl_failed"));
        }

        // 方法2: 直接写发行版配置文件
        std::string conf_file;
        if (cfg_.linux_distro == "debian") {
            conf_file = "/etc/default/locale";
        } else if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
            conf_file = "/etc/locale.conf";
        } else {
            // 尝试 /etc/default/locale 和 /etc/locale.conf，哪个存在写哪个
            if (fs::exists("/etc/default/locale")) conf_file = "/etc/default/locale";
            else if (fs::exists("/etc/locale.conf")) conf_file = "/etc/locale.conf";
            else conf_file = "/etc/default/locale";
        }

        Logger::step(_f("env.locale_writing_config", conf_file));
        auto r = Executor::shell(
            "echo 'LANG=" + locale_str + "' > " + conf_file + " && "
            "echo 'LC_ALL=" + locale_str + "' >> " + conf_file);
        if (r.ok()) {
            Logger::ok(_f("env.locale_written", conf_file));
            return true;
        }

        Logger::error(_f("env.locale_write_failed", conf_file));
        return false;
    }

    void Environment::open_uri(const std::string &uri) const {
        if (cfg_.is_termux) {
            Logger::step(_f("env.launching_intent", uri));
            Executor::shell("am start -a android.intent.action.VIEW -d \"" + uri + "\" >/dev/null 2>&1");
        } else {
            Logger::step(_f("env.launching_program", uri));
            // 从 root 降权回真实用户以启动 UI 程序
            std::string sudo_user = std::getenv("SUDO_USER") ? std::getenv("SUDO_USER") : "";
            std::string cmd;
            if (!sudo_user.empty()) {
                cmd = "su - " + sudo_user + " -c \"xdg-open '" + uri + "'\"";
            } else {
                cmd = "xdg-open \"" + uri + "\"";
            }
            // 将打开器放入后台，避免阻塞容器生命周期
            Executor::shell(cmd + " >/dev/null 2>&1 &");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // locale 辅助方法
    // ═══════════════════════════════════════════════════════════════

    bool Environment::is_locale_generated(std::string_view lang) const {
        return CommandBuilder("locale").add_flag("-a")
            .add_raw("2>/dev/null | grep -qi '^" + std::string(lang) + "'")
            .execute().ok();
    }

    bool Environment::generate_locale_auto(std::string_view lang) {
        std::string locale_str = std::string(lang) + ".UTF-8";

        if (cfg_.linux_distro == "debian") {
            return CommandBuilder("locale-gen").add_arg(locale_str)
                .add_raw("2>/dev/null").execute().ok();
        } else if (cfg_.linux_distro == "arch" || cfg_.linux_distro == "gentoo") {
            return generate_locale_via_locale_gen(lang);
        } else if (cfg_.linux_distro == "alpine") {
            // musl libc: locale 内置于 libc，确保 musl-locales 已安装即可
            return Executor::shell("apk info -e musl-locales 2>/dev/null").ok() ||
                   Executor::passthrough(cfg_.install_command + " musl-locales 2>/dev/null").ok();
        }
        // openwrt / unknown: musl 或未知环境，设置 LANG 即可
        return true;
    }

    bool Environment::generate_locale_debian_interactive() {
        Logger::info(_("env.dpkg_reconfigure_start"));
        Logger::info(_("env.dpkg_reconfigure_info"));
        auto result = Executor::passthrough("sudo dpkg-reconfigure locales");
        return result.ok();
    }

    bool Environment::generate_locale_via_locale_gen(std::string_view lang) {
        std::string locale_str = std::string(lang) + ".UTF-8";
        // 取消 /etc/locale.gen 中对应行的注释
        std::string sed_cmd = "sudo sed -i 's/^#\\(" + locale_str + "\\)/\\1/' /etc/locale.gen 2>/dev/null";
        Executor::shell(sed_cmd);
        return Executor::shell("locale-gen 2>/dev/null").ok();
    }

    // ═══════════════════════════════════════════════════════════════
    // 字体辅助方法
    // ═══════════════════════════════════════════════════════════════

    bool Environment::has_font_coverage(std::string_view lang) const {
        std::string fc_code = lang_to_fc_code(lang);
        if (fc_code.empty()) return true; // 拉丁语言默认系统已覆盖

        if (!Executor::has("fc-list")) return false;

        auto result = CommandBuilder("fc-list").add_arg(":lang=" + fc_code)
            .add_raw("2>/dev/null | head -1").execute();
        return result.ok() && !result.stdout_data.empty();
    }

    bool Environment::install_font_packages() {
        Logger::step(_("env.installing_fonts"));

        if (cfg_.linux_distro == "openwrt") {
            Logger::warn(_("env.font_skip_openwrt"));
            return true;
        }

        bool ok = false;
        if (cfg_.linux_distro == "debian") {
            ok = Executor::passthrough(cfg_.install_command +
                                       " fonts-noto-cjk fonts-noto 2>/dev/null").ok() ||
                 Executor::passthrough(cfg_.install_command +
                                       " fonts-noto-cjk 2>/dev/null").ok() ||
                 Executor::passthrough(cfg_.install_command +
                                       " wqy-microhei 2>/dev/null").ok();
        } else if (cfg_.linux_distro == "arch") {
            ok = Executor::passthrough(cfg_.install_command +
                                       " noto-fonts-cjk noto-fonts 2>/dev/null").ok() ||
                 Executor::passthrough(cfg_.install_command +
                                       " noto-fonts-cjk 2>/dev/null").ok();
        } else if (cfg_.linux_distro == "alpine") {
            ok = Executor::passthrough(cfg_.install_command +
                                       " font-noto-cjk font-noto 2>/dev/null").ok() ||
                 Executor::passthrough(cfg_.install_command +
                                       " font-noto-cjk 2>/dev/null").ok();
        } else if (cfg_.linux_distro == "gentoo") {
            ok = Executor::passthrough(
                     "sudo emerge --ask=n media-fonts/noto-cjk media-fonts/noto 2>/dev/null").ok() ||
                 Executor::passthrough(
                     "sudo emerge --ask=n media-fonts/noto-cjk 2>/dev/null").ok();
        } else {
            // 未知发行版：尝试常见包管理器
            ok = PackageManager::install({"fonts-noto-cjk", "fonts-noto"}, DistroFamily::Debian) ||
                 PackageManager::install({"noto-fonts-cjk", "noto-fonts"}, DistroFamily::Arch) ||
                 PackageManager::install({"font-noto-cjk", "font-noto"}, DistroFamily::Alpine);
        }

        if (ok) {
            Logger::ok(_("env.font_install_ok"));
        } else {
            Logger::warn(_("env.font_install_failed"));
        }
        return ok;
    }

    void Environment::refresh_font_cache() const {
        Executor::passthrough("fc-cache -fv 2>/dev/null || true");
    }

    // ═══════════════════════════════════════════════════════════════
    // 语言—字形映射
    // ═══════════════════════════════════════════════════════════════

    std::string Environment::lang_to_fc_code(std::string_view lang) {
        if (lang == "zh_CN") return "zh";
        return ""; // 拉丁语言不需要特殊检测
    }

    bool Environment::needs_cjk(std::string_view lang) {
        return lang == "zh_CN";
    }

    bool Environment::needs_cyrillic(std::string_view lang) {
        (void) lang;
        return false; // 当前仅支持中/英文，无需西里尔字形
    }
} // namespace tmoe::domain
