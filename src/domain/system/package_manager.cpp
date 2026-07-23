#include "domain/system/package_manager.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/str_utils.h"
#include "core/platform.h"

namespace tmoe::domain {

    /** 非 root 用户需要 sudo 提权，root/Termux 则不需要 */
    static std::string maybe_sudo(const std::string &cmd) {
        if (platform::geteuid() != 0) return "sudo " + cmd;
        return cmd;
    }

    DistroFamily PackageManager::detect_distro_family() {
        // 通过 /etc/os-release 检测
        auto result = Executor::shell("cat /etc/os-release 2>/dev/null").stdout_data;
        if (contains(result, "debian") ||
            contains(result, "ubuntu") ||
            contains(result, "deepin") ||
            contains(result, "Kali") ||
            contains(result, "uos.com")) {
            return DistroFamily::Debian;
        }
        if (contains(result, "Arch") ||
            contains(result, "Manjaro")) {
            return DistroFamily::Arch;
        }
        if (contains(result, "Fedora") ||
            contains(result, "CentOS") ||
            contains(result, "Red Hat") ||
            contains(result, "redhat")) {
            return DistroFamily::RedHat;
        }
        if (contains(result, "Alpine")) {
            return DistroFamily::Alpine;
        }
        if (contains(result, "gentoo") ||
            contains(result, "funtoo")) {
            return DistroFamily::Gentoo;
        }
        if (contains(result, "Solus")) {
            return DistroFamily::Solus;
        }
        if (contains(result, "void")) {
            return DistroFamily::Void_;
        }
        if (contains(result, "openSUSE") ||
            contains(result, "suse")) {
            return DistroFamily::Suse;
        }
        if (contains(result, "Slackware")) {
            return DistroFamily::Slackware;
        }
        if (contains(result, "openwrt")) {
            return DistroFamily::OpenWrt;
        }

        // 回退: 通过包管理器命令检测
        if (is_command_available("dpkg") && is_command_available("apt-cache"))
            return DistroFamily::Debian;
        if (is_command_available("pacman"))
            return DistroFamily::Arch;

        return DistroFamily::Unknown;
    }

    PackageManager::Commands PackageManager::commands_for(DistroFamily family) {
        switch (family) {
            case DistroFamily::Debian:
                return {"eatmydata apt install -y", "apt purge -y", "apt update", true};
            case DistroFamily::Arch:
                return {"pacman -Syu --noconfirm --needed", "pacman -Rsc", "pacman -Syy", false};
            case DistroFamily::RedHat:
                if (is_command_available("dnf"))
                    return {"dnf install -y --skip-broken", "dnf remove -y", "dnf update", false};
                return {"yum install -y --skip-broken", "yum remove -y", "yum update", false};
            case DistroFamily::Alpine:
                return {"apk add", "apk del", "apk update", false};
            case DistroFamily::Gentoo:
                return {"emerge -avk", "emerge -C", "", false};
            case DistroFamily::Suse:
                return {"zypper in -y", "zypper rm", "", false};
            case DistroFamily::Solus:
                return {"eopkg install -y", "eopkg remove -y", "", false};
            case DistroFamily::Void_:
                return {"xbps-install -Sy", "xbps-remove -R", "", false};
            case DistroFamily::Slackware:
                return {"slackpkg install", "slackpkg remove", "slackpkg update", false};
            case DistroFamily::OpenWrt:
                return {"opkg install", "opkg remove", "opkg update", false};
            default:
                return {"apt install -y", "apt purge -y", "apt update", false};
        }
    }

    // ── 自动检测发行版的便捷重载 ──

    bool PackageManager::install(std::string_view pkg) {
        return install(std::string(pkg), detect_distro_family());
    }

    bool PackageManager::install(const std::vector<std::string> &pkgs) {
        return install(pkgs, detect_distro_family());
    }

    bool PackageManager::remove(std::string_view pkg) {
        return remove(std::string(pkg), detect_distro_family());
    }

    bool PackageManager::remove(const std::vector<std::string> &pkgs) {
        return remove(pkgs, detect_distro_family());
    }

    bool PackageManager::update() {
        return update(detect_distro_family());
    }

    // ── 指定发行版的原始重载 ──

    bool PackageManager::install(std::string_view pkg, DistroFamily family) {
        return install(std::vector<std::string>{std::string(pkg)}, family);
    }

    bool PackageManager::install(const std::vector<std::string> &pkgs, DistroFamily family) {
        if (pkgs.empty()) return true;
        auto cmd = commands_for(family);

        if (cmd.use_eatmydata) {
            ensure_eatmydata();
        }

        // 拼装包名列表供显示
        std::string pkg_list;
        for (const auto &p: pkgs) {
            if (!pkg_list.empty()) pkg_list += " ";
            pkg_list += p;
        }
        Logger::step(_("app.installing") + ": " + pkg_list);

        std::string install_line = build_install_cmd(pkgs, cmd);

        // 透传输出，让用户看到包管理器的实时进度
        if (Executor::passthrough(maybe_sudo(install_line)).ok()) {
            Logger::ok(_("app.install_done"));
            return true;
        }

        // 容错：Debian 下尝试不带 eatmydata
        if (cmd.use_eatmydata) {
            Logger::warn(_("app.retry_without_eatmydata"));
            std::string fallback = maybe_sudo("apt install -y ");
            for (const auto &p: pkgs) fallback += p + " ";
            if (Executor::passthrough(fallback).ok()) return true;
        }

        Logger::error(_("app.install_failed"));
        return false;
    }

    bool PackageManager::remove(std::string_view pkg, DistroFamily family) {
        return remove(std::vector<std::string>{std::string(pkg)}, family);
    }

    bool PackageManager::remove(const std::vector<std::string> &pkgs, DistroFamily family) {
        if (pkgs.empty()) return true;
        auto cmd = commands_for(family);

        std::string pkg_list;
        for (const auto &p: pkgs) {
            if (!pkg_list.empty()) pkg_list += " ";
            pkg_list += p;
        }
        Logger::step(_("app.removing") + ": " + pkg_list);

        std::string remove_line = build_remove_cmd(pkgs, cmd);
        return Executor::passthrough(maybe_sudo(remove_line)).ok();
    }

    bool PackageManager::update(DistroFamily family) {
        auto cmd = commands_for(family);
        if (cmd.update.empty()) return true;
        Logger::step(_("app.updating"));
        return Executor::passthrough(maybe_sudo(cmd.update)).ok();
    }

    bool PackageManager::is_command_available(std::string_view cmd) {
        return Executor::shell("command -v " + std::string(cmd) + " 2>/dev/null").ok();
    }

    bool PackageManager::ensure_eatmydata() {
        if (is_command_available("eatmydata")) return true;
        Logger::step(_("app.installing_eatmydata"));
        if (Executor::passthrough(maybe_sudo("apt install -y eatmydata")).ok()) return true;
        return Executor::passthrough(maybe_sudo("apt install -y -f eatmydata")).ok();
    }

    std::string PackageManager::family_key(DistroFamily family) {
        switch (family) {
            case DistroFamily::Debian:
                return "debian";
            case DistroFamily::Arch:
                return "arch";
            case DistroFamily::RedHat:
                return "redhat";
            case DistroFamily::Void_:
                return "void";
            case DistroFamily::Gentoo:
                return "gentoo";
            case DistroFamily::Suse:
                return "suse";
            case DistroFamily::Alpine:
                return "alpine";
            case DistroFamily::Solus:
                return "solus";
            default:
                return "";
        }
    }

    std::string PackageManager::build_install_cmd(const std::vector<std::string> &pkgs,
                                                  const Commands &cmd) {
        std::string result = cmd.install;
        for (const auto &p: pkgs) {
            result += " " + p;
        }
        return result;
    }

    std::string PackageManager::build_remove_cmd(const std::vector<std::string> &pkgs,
                                                 const Commands &cmd) {
        std::string result = cmd.remove;
        for (const auto &p: pkgs) {
            result += " " + p;
        }
        return result;
    }

    DistroFamily PackageManager::resolve_family(const std::string &distro_name) {
        auto family = infer_family_from_config(distro_name);
        if (family == DistroFamily::Unknown)
            family = detect_distro_family();
        return family;
    }

    DistroFamily infer_family_from_config(const std::string &distro_name) {
        auto lower = distro_name;
        for (auto &c: lower) c = static_cast<char>(std::tolower(c));

        if (contains(lower, "debian") ||
            contains(lower, "ubuntu") ||
            contains(lower, "deepin") ||
            contains(lower, "kali"))
            return DistroFamily::Debian;
        if (contains(lower, "arch") || contains(lower, "manjaro"))
            return DistroFamily::Arch;
        if (contains(lower, "fedora") ||
            contains(lower, "centos") ||
            contains(lower, "redhat"))
            return DistroFamily::RedHat;
        if (contains(lower, "alpine"))
            return DistroFamily::Alpine;
        if (contains(lower, "gentoo"))
            return DistroFamily::Gentoo;
        if (contains(lower, "solus"))
            return DistroFamily::Solus;
        if (contains(lower, "void"))
            return DistroFamily::Void_;
        if (contains(lower, "suse") || contains(lower, "opensuse"))
            return DistroFamily::Suse;
        if (contains(lower, "slackware"))
            return DistroFamily::Slackware;
        if (contains(lower, "openwrt"))
            return DistroFamily::OpenWrt;

        return DistroFamily::Unknown;
    }
} // namespace tmoe::domain
