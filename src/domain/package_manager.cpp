#include "package_manager.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"

namespace tmoe::domain {
    DistroFamily PackageManager::detect_distro_family() {
        // 通过 /etc/os-release 检测
        auto result = Executor::shell("cat /etc/os-release 2>/dev/null").stdout_data;
        if (result.find("debian") != std::string::npos ||
            result.find("ubuntu") != std::string::npos ||
            result.find("deepin") != std::string::npos ||
            result.find("Kali") != std::string::npos ||
            result.find("uos.com") != std::string::npos) {
            return DistroFamily::Debian;
        }
        if (result.find("Arch") != std::string::npos ||
            result.find("Manjaro") != std::string::npos) {
            return DistroFamily::Arch;
        }
        if (result.find("Fedora") != std::string::npos ||
            result.find("CentOS") != std::string::npos ||
            result.find("Red Hat") != std::string::npos ||
            result.find("redhat") != std::string::npos) {
            return DistroFamily::RedHat;
        }
        if (result.find("Alpine") != std::string::npos) {
            return DistroFamily::Alpine;
        }
        if (result.find("gentoo") != std::string::npos ||
            result.find("funtoo") != std::string::npos) {
            return DistroFamily::Gentoo;
        }
        if (result.find("Solus") != std::string::npos) {
            return DistroFamily::Solus;
        }
        if (result.find("void") != std::string::npos) {
            return DistroFamily::Void_;
        }
        if (result.find("openSUSE") != std::string::npos ||
            result.find("suse") != std::string::npos) {
            return DistroFamily::Suse;
        }
        if (result.find("Slackware") != std::string::npos) {
            return DistroFamily::Slackware;
        }
        if (result.find("openwrt") != std::string::npos) {
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
                return {"apk add", "sudo apk del", "apk update", false};
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
        if (Executor::passthrough(install_line).ok()) {
            Logger::ok(_("app.install_done"));
            return true;
        }

        // 容错：Debian 下尝试不带 eatmydata
        if (cmd.use_eatmydata) {
            Logger::warn(_("app.retry_without_eatmydata"));
            std::string fallback = "apt install -y ";
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
        return Executor::passthrough(remove_line).ok();
    }

    bool PackageManager::update(DistroFamily family) {
        auto cmd = commands_for(family);
        if (cmd.update.empty()) return true;
        Logger::step(_("app.updating"));
        return Executor::passthrough(cmd.update).ok();
    }

    bool PackageManager::is_command_available(std::string_view cmd) {
        return Executor::shell("command -v " + std::string(cmd) + " 2>/dev/null").ok();
    }

    bool PackageManager::ensure_eatmydata() {
        if (is_command_available("eatmydata")) return true;
        Logger::step(_("app.installing_eatmydata"));
        if (Executor::passthrough("apt install -y eatmydata").ok()) return true;
        return Executor::passthrough("apt install -y -f eatmydata").ok();
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

    DistroFamily infer_family_from_config(const std::string &distro_name) {
        auto lower = distro_name;
        for (auto &c: lower) c = static_cast<char>(std::tolower(c));

        if (lower.find("debian") != std::string::npos ||
            lower.find("ubuntu") != std::string::npos ||
            lower.find("deepin") != std::string::npos ||
            lower.find("kali") != std::string::npos)
            return DistroFamily::Debian;
        if (lower.find("arch") != std::string::npos || lower.find("manjaro") != std::string::npos)
            return DistroFamily::Arch;
        if (lower.find("fedora") != std::string::npos ||
            lower.find("centos") != std::string::npos ||
            lower.find("redhat") != std::string::npos)
            return DistroFamily::RedHat;
        if (lower.find("alpine") != std::string::npos)
            return DistroFamily::Alpine;
        if (lower.find("gentoo") != std::string::npos)
            return DistroFamily::Gentoo;
        if (lower.find("solus") != std::string::npos)
            return DistroFamily::Solus;
        if (lower.find("void") != std::string::npos)
            return DistroFamily::Void_;
        if (lower.find("suse") != std::string::npos || lower.find("opensuse") != std::string::npos)
            return DistroFamily::Suse;
        if (lower.find("slackware") != std::string::npos)
            return DistroFamily::Slackware;
        if (lower.find("openwrt") != std::string::npos)
            return DistroFamily::OpenWrt;

        return DistroFamily::Unknown;
    }
} // namespace tmoe::domain
