#include "domain/system/mirrors.h"
#include "core/i18n.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace fs = std::filesystem;

namespace tmoe::domain {
    MirrorManager::MirrorManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    std::vector<MirrorEntry> MirrorManager::available() const {
        return MirrorRegistry::instance().all();
    }

    std::vector<MirrorEntry> MirrorManager::by_category(const std::string &cat) const {
        return MirrorRegistry::instance().by_category(cat);
    }

    /** 检测当前发行版的版本代号。
     *  读取 /etc/os-release 中的 VERSION_CODENAME；失败时回退到 PRETTY_NAME。
     */
    std::string MirrorManager::detect_version_codename() const {
        const auto &distro = cfg_.linux_distro;

        // Debian / Ubuntu / Kali: 读取 VERSION_CODENAME
        if (distro == "debian" || distro == "ubuntu" || distro == "kali") {
            auto r = Executor::shell(
                "grep VERSION_CODENAME /etc/os-release 2>/dev/null | cut -d= -f2");
            if (r.ok() && !r.stdout_data.empty()) {
                // 去掉末尾换行符
                auto s = r.stdout_data;
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                if (!s.empty()) return s;
            }
            // 回退: /etc/os-release PRETTY_NAME
            r = Executor::shell(
                "grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"'");
            if (r.ok() && !r.stdout_data.empty()) {
                auto s = r.stdout_data;
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                Logger::warn(_f("mirror.no_codename", s));
                return s;
            }
            Logger::error(_("mirror.cannot_detect_version"));
            return "sid";
        }

        // Alpine: 从 PRETTY_NAME 提取版本号
        if (distro == "alpine") {
            auto r = Executor::shell(
                "grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"' | awk '{print $NF}'");
            if (r.ok() && !r.stdout_data.empty()) {
                auto s = r.stdout_data;
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                return s; // 例如 "v3.18"
            }
            return "latest-stable";
        }

        return "latest";
    }

    /** 在首次修改前创建原始源的一次性备份。 */
    bool MirrorManager::ensure_backup() const {
        auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
        std::string backup_path;
        std::string source_file;

        if (compat) {
            // 展开 ~ 为 $HOME
            std::string bp = compat->backup_path;
            if (!bp.empty() && bp[0] == '~') {
                bp = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + bp.substr(1);
            }
            backup_path = bp;
            source_file = compat->source_file;
        }

        if (backup_path.empty() || source_file.empty()) {
            Logger::debug(std::string(_("mirror.no_backup_config")) + " (" + cfg_.linux_distro + ")");
            return true;
        }

        // 一次性快照: 备份已存在则不覆盖
        if (fs::exists(backup_path)) {
            // Logger::debug("备份文件已存在，跳过: " + backup_path);
            return true;
        }

        // RedHat: 打包整个 yum.repos.d 目录
        if (cfg_.linux_distro == "redhat") {
            Logger::step(_("mirror.backing_up_redhat"));
            auto r = Executor::shell("mkdir -p \"$(dirname " + backup_path + ")\" && "
                                     "tar -Ppzcvf " + backup_path + " " + compat->source_dir);
            Logger::ok_or_fail(r.ok(), "备份 yum.repos.d");
            return r.ok();
        }

        // 普通文件拷贝
        Logger::step(_("mirror.backing_up"));
        auto r = Executor::shell("mkdir -p \"$(dirname " + backup_path + ")\" && "
                                 "cp -pf " + source_file + " " + backup_path);
        Logger::ok_or_fail(r.ok(), "备份 " + source_file);

        // Arch: 额外备份 pacman.conf
        if (cfg_.linux_distro == "arch" && compat && !compat->backup_conf.empty()) {
            std::string bc = compat->backup_conf;
            if (!bc.empty() && bc[0] == '~') {
                bc = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + bc.substr(1);
            }
            if (!fs::exists(bc)) {
                Executor::shell("cp -pf " + compat->source_conf + " " + bc);
            }
        }
        return r.ok();
    }

    // ── 软件包数据库更新与升级 ──
    bool MirrorManager::run_update() const {
        Logger::step(_("mirror.updating"));
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
            return Executor::passthrough("apt update").ok();
        }
        if (cfg_.linux_distro == "arch") {
            return Executor::passthrough("pacman -Syyu --noconfirm").ok();
        }
        if (cfg_.linux_distro == "alpine") {
            return Executor::passthrough("apk update").ok();
        }
        return true;
    }

    bool MirrorManager::run_dist_upgrade() const {
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
            return Executor::passthrough("apt dist-upgrade -y").ok();
        }
        if (cfg_.linux_distro == "arch") {
            return Executor::passthrough("pacman -Su --noconfirm").ok();
        }
        if (cfg_.linux_distro == "alpine") {
            return Executor::passthrough("apk upgrade").ok();
        }
        return true;
    }

    bool MirrorManager::http_to_https_if_ca_available() const {
        if (cfg_.linux_distro != "debian" && cfg_.linux_distro != "ubuntu" && cfg_.linux_distro != "kali") {
            return true;
        }
        if (!fs::exists("/usr/sbin/update-ca-certificates")) {
            Logger::debug(std::string(_("mirror.no_ca_certs")));
            return true;
        }
        Logger::step(_("mirror.ca_to_https"));
        Executor::shell("sed -i 's@http://@https://@g' /etc/apt/sources.list");
        Executor::shell("sed -i 's@https://security@http://security@g' /etc/apt/sources.list");
        return true;
    }

    std::string MirrorManager::resolve_hostname(const std::string &url) const {
        // 从 URL 中提取纯主机名（去掉协议和路径）
        std::string host = url;
        auto pos = host.find("://");
        if (pos != std::string::npos) host = host.substr(pos + 3);
        pos = host.find('/');
        if (pos != std::string::npos) host = host.substr(0, pos);

        // 方法1: getent hosts (glibc 内置，依赖最少)
        auto r = Executor::shell("getent hosts " + host + " 2>/dev/null | awk '{print $1; exit}'");
        if (r.ok() && !r.stdout_data.empty()) {
            auto ip = r.stdout_data;
            while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r')) ip.pop_back();
            return ip;
        }

        // 方法2: python3 socket.gethostbyname 回退方案
        r = Executor::shell(
            "python3 -c \"import socket; print(socket.gethostbyname('" + host + "'))\" 2>/dev/null");
        if (r.ok() && !r.stdout_data.empty()) {
            auto ip = r.stdout_data;
            while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r')) ip.pop_back();
            return ip;
        }

        return ""; // 所有方法均解析失败
    }

    // ── 各发行版源写入器（对齐原版 Bash heredoc 模板）──
    bool MirrorManager::write_debian_sources(const MirrorEntry &mirror) {
        std::string codename = detect_version_codename();
        std::string url = mirror.url;

        Logger::info(_f("mirror.switching_debian", codename, mirror.name));

        // 注释掉所有旧 deb 行
        Executor::shell("sed -i 's/^deb/# &/g' /etc/apt/sources.list");

        // 判断旧格式 vs 新格式（buster/stretch/jessie 沿用旧格式）
        bool old_style = (codename == "buster" || codename == "stretch" || codename == "jessie");

        if (codename == "sid") {
            // sid: 简化格式
            Executor::shell("cat >>/etc/apt/sources.list <<-'TMOE_EOF'\n"
                            "deb http://" + url + "/debian/ sid main contrib non-free\n"
                            "#deb http://" + url + "/debian/ experimental main contrib non-free\n"
                            "TMOE_EOF");
        } else if (old_style) {
            // 旧版 security 路径: xxx/updates
            Executor::shell("cat >>/etc/apt/sources.list <<-TMOE_EOF\n"
                            "deb http://" + url + "/debian/ " + codename + " main contrib non-free\n"
                            "deb http://" + url + "/debian/ " + codename + "-updates main contrib non-free\n"
                            "deb http://" + url + "/debian/ " + codename + "-backports main contrib non-free\n"
                            "deb http://" + url + "/debian-security/ " + codename + "/updates main contrib non-free\n"
                            "TMOE_EOF");
        } else {
            // 新版 Debian 格式
            Executor::shell("cat >>/etc/apt/sources.list <<-TMOE_EOF\n"
                            "deb http://" + url + "/debian/ " + codename + " main contrib non-free\n"
                            "deb http://" + url + "/debian/ " + codename + "-updates main contrib non-free\n"
                            "deb http://" + url + "/debian/ " + codename + "-backports main contrib non-free\n"
                            "deb http://" + url + "/debian-security/ " + codename + "-security main contrib non-free\n"
                            "TMOE_EOF");
        }

        return true;
    }

    bool MirrorManager::write_ubuntu_sources(const MirrorEntry &mirror) {
        std::string codename = detect_version_codename();
        std::string url = mirror.url;

        // 兼容性检测: ftp.*.debian.org 等镜像只托管 Debian 包，不适用于 Ubuntu
        if (url.find(".debian.org") != std::string::npos) {
            Logger::warn(_f("mirror.debian_only_ubuntu", mirror.name));
            Logger::warn(_("mirror.suggest_multi_distro_mirror"));
            if (!Logger::confirm(_("mirror.confirm_use_incompatible"))) {
                return false;
            }
        }

        Logger::info(_f("mirror.switching_ubuntu", codename, mirror.name));

        // 注释旧行 → 写入新模板
        Executor::shell("sed -i 's/^deb/# &/g' /etc/apt/sources.list");

        Executor::shell("cat >>/etc/apt/sources.list <<-TMOE_EOF\n"
                        "deb http://" + url + "/ubuntu/ " + codename + " main restricted universe multiverse\n"
                        "deb http://" + url + "/ubuntu/ " + codename + "-updates main restricted universe multiverse\n"
                        "deb http://" + url + "/ubuntu/ " + codename +
                        "-backports main restricted universe multiverse\n"
                        "deb http://" + url + "/ubuntu/ " + codename + "-security main restricted universe multiverse\n"
                        "TMOE_EOF");

        // ARM: 自动切换到 ubuntu-ports
        if (cfg_.arch != "amd64" && cfg_.arch != "i386" && cfg_.arch != "x86_64") {
            Logger::step(_("mirror.switch_to_ubuntu_ports"));
            Executor::shell("sed -i 's:/ubuntu:/ubuntu-ports:g' /etc/apt/sources.list");
        }

        return true;
    }

    bool MirrorManager::write_kali_sources(const MirrorEntry &mirror) {
        std::string url = mirror.url;

        // 兼容性检测: ftp.*.debian.org 等镜像只托管 Debian 包，不适用于 Kali
        if (url.find(".debian.org") != std::string::npos) {
            Logger::warn(_f("mirror.debian_only_kali", mirror.name));
            Logger::warn(_("mirror.suggest_multi_distro_mirror_kali"));
            if (!Logger::confirm(_("mirror.confirm_use_incompatible"))) {
                return false;
            }
        }

        Logger::info(_f("mirror.switching_kali", mirror.name));

        Executor::shell("sed -i 's/^deb/# &/g' /etc/apt/sources.list");

        auto compat = MirrorRegistry::instance().compat_for("kali");
        std::string branch = compat ? compat->rolling_branch : "kali-rolling";

        Executor::shell("cat >>/etc/apt/sources.list <<-TMOE_EOF\n"
                        "deb http://" + url + "/kali/ " + branch + " main contrib non-free\n"
                        "# deb http://" + url + "/debian/ stable main contrib non-free\n"
                        "# deb http://" + url + "/kali/ kali-last-snapshot main contrib non-free\n"
                        "TMOE_EOF");

        return true;
    }

    bool MirrorManager::write_arch_sources(const MirrorEntry &mirror) {
        std::string url = mirror.url;
        Logger::info(_f("mirror.switching_arch", mirror.name));

        // 注释掉所有旧 Server 行
        Executor::shell("sed -i 's/^Server/#&/g' /etc/pacman.d/mirrorlist");

        // 架构路由
        if (cfg_.arch == "arm64" || cfg_.arch == "armhf" || cfg_.arch == "aarch64") {
            Executor::shell("cat >>/etc/pacman.d/mirrorlist <<-TMOE_EOF\n"
                            "Server = https://" + url + "/archlinuxarm/$arch/$repo\n"
                            "TMOE_EOF");
        } else {
            Executor::shell("cat >>/etc/pacman.d/mirrorlist <<-TMOE_EOF\n"
                            "#Server = http://mirrors.kernel.org/archlinux/$repo/os/$arch\n"
                            "Server = https://" + url + "/archlinux/$repo/os/$arch\n"
                            "TMOE_EOF");
        }

        return true;
    }

    bool MirrorManager::write_alpine_sources(const MirrorEntry &mirror) {
        std::string version = detect_version_codename();
        std::string url = mirror.url;

        Logger::info(_f("mirror.switching_alpine", version, mirror.name));

        // Alpine: 注释所有旧 http 行
        Executor::shell("cd /etc/apk/ && sed -i 's@^http@#&@g' repositories");

        // 写入新仓库
        Executor::shell("cat >>/etc/apk/repositories <<-TMOE_EOF\n"
                        "http://" + url + "/alpine/" + version + "/main\n"
                        "http://" + url + "/alpine/" + version + "/community\n"
                        "TMOE_EOF");

        return true;
    }

    // ── Manjaro 源写入 ──
    bool MirrorManager::write_manjaro_sources(const MirrorEntry &mirror) {
        std::string url = mirror.url;
        Logger::info(_f("mirror.switching_manjaro", mirror.name));

        // 注释所有旧 Server 行
        Executor::shell("sed -i 's/^Server/#&/g' /etc/pacman.d/mirrorlist");

        // Manjaro 路径与 Arch 不同: manjaro/stable/$repo/$arch 或 manjaro/arm-stable/$repo/$arch
        if (cfg_.arch == "arm64" || cfg_.arch == "armhf" || cfg_.arch == "aarch64") {
            Executor::shell("cat >>/etc/pacman.d/mirrorlist <<-TMOE_EOF\n"
                            "#Server = https://" + url + "/archlinuxarm/$arch/$repo\n"
                            "Server = https://" + url + "/manjaro/arm-stable/$repo/$arch\n"
                            "TMOE_EOF");
        } else {
            Executor::shell("cat >>/etc/pacman.d/mirrorlist <<-TMOE_EOF\n"
                            "#Server = https://" + url + "/archlinux/$repo/os/$arch\n"
                            "Server = https://" + url + "/manjaro/stable/$repo/$arch\n"
                            "TMOE_EOF");
        }

        return true;
    }

    // ── RedHat/Fedora 源写入 ──
    bool MirrorManager::write_redhat_sources(const MirrorEntry &mirror) {
        std::string url = mirror.url;
        Logger::info(_f("mirror.switching_redhat", mirror.name));

        // 备份整个 yum.repos.d 目录
        ensure_backup();

        int fedora_ver = detect_fedora_version();
        if (fedora_ver <= 0) {
            Logger::error(_("mirror.cannot_detect_fedora"));
            return false;
        }

        if (fedora_ver >= 32) {
            fedora_32_repos(url);
            fedora_3x_repos(url);
        } else if (fedora_ver >= 30) {
            fedora_31_repos(url);
            fedora_3x_repos(url);
        } else {
            Logger::error(_("mirror.unsupported_fedora"));
            return false;
        }

        Executor::shell("dnf makecache 2>/dev/null || yum makecache 2>/dev/null || true");
        return true;
    }

    int MirrorManager::detect_fedora_version() const {
        auto r = Executor::shell("grep VERSION_ID /etc/os-release 2>/dev/null | cut -d= -f2");
        if (!r.ok() || r.stdout_data.empty()) return -1;
        try {
            return std::stoi(r.stdout_data);
        } catch (...) {
            return -1;
        }
    }

    void MirrorManager::fedora_31_repos(const std::string &url) {
        // 直接从镜像站下载官方 repo 文件
        Executor::shell("curl -o /etc/yum.repos.d/fedora.repo http://" + url + "/repo/fedora.repo 2>/dev/null || true");
        Executor::shell(
            "curl -o /etc/yum.repos.d/fedora-updates.repo http://" + url +
            "/repo/fedora-updates.repo 2>/dev/null || true");
    }

    void MirrorManager::fedora_32_repos(const std::string &url) {
        Executor::shell("cat >/etc/yum.repos.d/fedora.repo <<-'TMOE_EOF'\n"
                        "[fedora]\n"
                        "name=Fedora $releasever - $basearch\n"
                        "failovermethod=priority\n"
                        "baseurl=https://" + url + "/fedora/releases/$releasever/Everything/$basearch/os/\n"
                        "metadata_expire=28d\n"
                        "gpgcheck=1\n"
                        "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-$basearch\n"
                        "skip_if_unavailable=False\n"
                        "TMOE_EOF");
        Executor::shell("cat >/etc/yum.repos.d/fedora-updates.repo <<-'TMOE_EOF'\n"
                        "[updates]\n"
                        "name=Fedora $releasever - $basearch - Updates\n"
                        "failovermethod=priority\n"
                        "baseurl=https://" + url + "/fedora/updates/$releasever/Everything/$basearch/\n"
                        "enabled=1\n"
                        "gpgcheck=1\n"
                        "metadata_expire=6h\n"
                        "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-$basearch\n"
                        "skip_if_unavailable=False\n"
                        "TMOE_EOF");
    }

    void MirrorManager::fedora_3x_repos(const std::string &url) {
        Executor::shell("cat >/etc/yum.repos.d/fedora-modular.repo <<-'TMOE_EOF'\n"
                        "[fedora-modular]\n"
                        "name=Fedora Modular $releasever - $basearch\n"
                        "failovermethod=priority\n"
                        "baseurl=https://" + url + "/fedora/releases/$releasever/Modular/$basearch/os/\n"
                        "enabled=1\n"
                        "metadata_expire=7d\n"
                        "gpgcheck=1\n"
                        "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-$basearch\n"
                        "skip_if_unavailable=False\n"
                        "TMOE_EOF");
        Executor::shell("cat >/etc/yum.repos.d/fedora-updates-modular.repo <<-'TMOE_EOF'\n"
                        "[updates-modular]\n"
                        "name=Fedora Modular $releasever - $basearch - Updates\n"
                        "failovermethod=priority\n"
                        "baseurl=https://" + url + "/fedora/updates/$releasever/Modular/$basearch/\n"
                        "enabled=1\n"
                        "gpgcheck=1\n"
                        "metadata_expire=6h\n"
                        "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-$basearch\n"
                        "skip_if_unavailable=False\n"
                        "TMOE_EOF");
    }

    // ── Solus 源写入 ──
    bool MirrorManager::write_solus_sources(const MirrorEntry &mirror) {
        std::string url = mirror.url;
        Logger::info(_f("mirror.switching_solus", mirror.name));

        // Solus 使用 eopkg，先移除旧 mirror 再添加新的
        std::string repo_url = "https://" + url + "/solus/packages/shannon/eopkg-index.xml.xz";

        Logger::step(_("mirror.adding_solus_source"));
        Executor::shell("eopkg remove-repo mirror 2>/dev/null || true");

        Logger::step(_("mirror.adding_solus_new"));
        auto r = Executor::shell("eopkg add-repo mirror " + repo_url);
        if (!r.ok()) {
            Logger::error(_("mirror.solus_add_failed"));
            return false;
        }

        Executor::shell("eopkg enable-repo mirror 2>/dev/null || true");
        Executor::shell("eopkg disable-repo Solus 2>/dev/null || true"); // 禁用官方源

        return true;
    }

    // ── 下载速度测试 ──
    bool MirrorManager::run_download_speed_test() {
        Logger::step(_("mirror.speedtest_header"));
        Logger::warn(_("mirror.speedtest_warning"));

        if (!Logger::confirm(_("mirror.confirm_speed_test"))) {
            return false;
        }

        // 选取中国区镜像站 + Debian 全球镜像站
        auto cn_mirrors = MirrorRegistry::instance().by_region("cn");
        auto global = MirrorRegistry::instance().by_region("global");

        struct SpeedTarget { std::string name; std::string host; };
        std::vector<SpeedTarget> targets;

        auto add_target = [&](const auto& m) {
            std::string host = m.url;
            auto pos = host.find("://");
            if (pos != std::string::npos) host = host.substr(pos + 3);
            pos = host.find('/');
            if (pos != std::string::npos) host = host.substr(0, pos);
            // 去重
            for (auto& t : targets) if (t.host == host) return;
            targets.push_back({m.name, host});
        };
        for (const auto &m: cn_mirrors) add_target(m);
        for (const auto &m: global) add_target(m);

        if (targets.empty()) {
            Logger::error(_("mirror.no_mirrors_available"));
            return false;
        }

        Logger::info(_("mirror.speedtest_test_file"));
        Logger::info(_f("mirror.speedtest_mirror_count", std::to_string(targets.size())));
        Logger::info("---------------------------");

        std::vector<std::pair<std::string, std::string>> results;

        for (const auto &t: targets) {
            // 用 curl -w 取下载速度: %{speed_download} = bytes/sec
            // --max-time 8 防止卡死, -o /dev/null 不存文件
            std::string test_url = "https://" + t.host + "/debian/ls-lR.gz";
            Logger::info(_f("mirror.speedtest_checking", t.name, t.host));

            auto r = Executor::shell(
                "curl -sL --max-time 8 -o /dev/null -w '%{speed_download}' "
                "\"" + test_url + "\" 2>/dev/null");
            if (r.ok() && !r.stdout_data.empty()) {
                std::string s = r.stdout_data;
                // 提取纯数字和小数点，去掉所有空白和垃圾字符
                s.erase(std::remove_if(s.begin(), s.end(),
                    [](unsigned char c){ return !std::isdigit(c) && c != '.'; }), s.end());
                if (!s.empty() && s != "0" && s != "0.000") {
                    double bps = 0;
                    try { bps = std::stod(s); } catch (...) {}
                    std::string human;
                    if (bps > 1048576) {
                        human = std::to_string((int)(bps / 1048576)) + " MiB/s";
                    } else if (bps > 1024) {
                        human = std::to_string((int)(bps / 1024)) + " KiB/s";
                    } else {
                        human = std::to_string((int)bps) + " B/s";
                    }
                    results.push_back({t.name, human});
                    Logger::info(_f("mirror.speedtest_result", human));
                } else {
                    Logger::warn(_("mirror.speedtest_failed_single"));
                }
            } else {
                Logger::warn(_("mirror.speedtest_failed_single"));
            }
        }

        Logger::info("---------------------------");
        if (!results.empty()) {
            // 按速度排序（简单提取数值比较）
            Logger::ok(_("mirror.speedtest_complete"));
            for (const auto& x : results)
                Logger::info("  " + x.first + " — " + x.second);
        }
        Logger::info(_("mirror.speedtest_tip"));
        return true;
    }

    // ── Ping 延迟对比测试 ──
    bool MirrorManager::run_ping_latency_test() {
        Logger::step(_("mirror.ping_test_title"));

        auto mirrors = MirrorRegistry::instance().all();
        if (mirrors.empty()) {
            Logger::error(_("mirror.ping_registry_empty"));
            return false;
        }

        // ═══ 第一步: 批量 DNS 预解析（避免每个 ping 各自等 DNS） ═══
        struct ResolvedTarget {
            std::string id;
            std::string name;
            std::string host;
            std::string ip;
        };
        std::vector<ResolvedTarget> resolved;

        Logger::info(_f("mirror.ping_resolving_dns", std::to_string(mirrors.size())));
        for (const auto &m: mirrors) {
            std::string host = m.url;
            auto pos = host.find("://");
            if (pos != std::string::npos) host = host.substr(pos + 3);
            pos = host.find('/');
            if (pos != std::string::npos) host = host.substr(0, pos);

            std::string ip = resolve_hostname(m.url);
            if (!ip.empty()) {
                resolved.push_back({m.id, m.name, host, ip});
            } else {
                Logger::warn(_f("mirror.ping_dns_failed_single", host));
            }
        }

        if (resolved.empty()) {
            Logger::error(_("mirror.ping_dns_all_failed"));
            return false;
        }

        // ═══ 第二步: 对已解析 IP 执行 ping（零 DNS 开销） ═══
        Logger::info(_f("mirror.ping_testing", std::to_string(resolved.size())));
        Logger::info(_("mirror.ping_separator"));

        struct PingResult {
            std::string id;
            std::string name;
            std::string host;
            double avg_ms;
        };
        std::vector<PingResult> results;

        for (const auto &rt: resolved) {
            auto r = Executor::shell(
                "ping -c 3 -W 2 " + rt.ip +
                " 2>/dev/null | grep 'rtt min/avg/max' | cut -d/ -f5");
            if (r.ok() && !r.stdout_data.empty()) {
                try {
                    double avg = std::stod(r.stdout_data);
                    results.push_back({rt.id, rt.name, rt.host, avg});
                    // 按延迟着色显示
                    std::string color = (avg < 30) ? "🟢" : (avg < 80) ? "🟡" : "🔴";
                    Logger::info("  " + color + " " + _f("mirror.ping_result_entry", rt.name, rt.host,
                                                         std::to_string(avg).substr(0, 6)));
                } catch (...) {
                    Logger::warn(_f("mirror.ping_result_parse_failed", rt.name));
                }
            } else {
                Logger::warn(_f("mirror.ping_unreachable", rt.name, rt.host));
            }
        }

        if (results.empty()) {
            Logger::warn(_("mirror.ping_all_failed"));
            return false;
        }

        // ═══ 第三步: 按延迟升序排名 ═══
        std::sort(results.begin(), results.end(),
                  [](const PingResult &a, const PingResult &b) { return a.avg_ms < b.avg_ms; });

        Logger::info("───────────────��───────────────────────────");
        Logger::info(_("mirror.ping_ranking"));
        for (size_t i = 0; i < results.size(); ++i) {
            std::string medal = (i == 0) ? "🥇" : (i == 1) ? "🥈" : (i == 2) ? "🥉" : "  ";
            Logger::info("  " + medal + " " + _f("mirror.ping_rank_entry", std::to_string(i + 1), results[i].name,
                                                 std::to_string(results[i].avg_ms).substr(0, 6)));
        }

        Logger::ok(_("mirror.ping_complete"));
        Logger::info(_("mirror.ping_tip"));
        return true;
    }

    // ── 额外源管理 ──
    bool MirrorManager::manage_extra_sources() {
        const auto &distro = cfg_.linux_distro;

        std::string menu = cfg_.tui_bin + " --title \"" + _("mirror.extra_title") + "\" "
                           "--menu \"" + _("mirror.extra_prompt") + "\" 0 0 0 ";

        if (distro == "debian") {
            menu += "\"1\" \"" + _("mirror.extra_debian_kali") + "\" ";
        } else if (distro == "arch") {
            menu += "\"1\" \"" + _("mirror.extra_archlinuxcn") + "\" ";
        } else if (distro == "redhat") {
            // 检测是 Fedora 还是 RHEL/CentOS
            int fv = detect_fedora_version();
            if (fv > 0) {
                menu += "\"1\" \"" + _("mirror.extra_rpmfusion") + "\" ";
            } else {
                menu += "\"1\" \"" + _("mirror.extra_epel") + "\" ";
            }
        } else {
            Logger::error(_f("mirror.extra_unsupported", distro));
            return false;
        }

        menu += "\"0\" \"" + _("menu.tui.back") + "\"";

        auto choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) return true;

        bool ok = false;
        if (distro == "debian") {
            ok = add_kali_extra_source();
        } else if (distro == "arch") {
            ok = add_archlinuxcn_source();
        } else if (distro == "redhat") {
            int fv = detect_fedora_version();
            if (fv > 0) ok = add_rpmfusion_source();
            else ok = add_epel_source();
        }

        if (ok) run_update();
        return ok;
    }

    // ── Kali 额外源 (Debian → Kali) ──
    bool MirrorManager::add_kali_extra_source() {
        std::string kali_list = "/etc/apt/sources.list.d/kali-linux.list";
        std::string keyring = "/usr/share/keyrings/kali-linux-archive-keyring.gpg";

        if (fs::exists(kali_list)) {
            Logger::warn(_("mirror.kali_list_detected"));
            if (!Logger::confirm(_("mirror.confirm_remove_kali_reconfig"))) return false;
            Executor::shell("rm -fv " + kali_list + " " + keyring);
        }

        Logger::step(_("mirror.kali_install_gnupg"));
        Executor::passthrough("apt install -y gnupg || true");

        Logger::step(_("mirror.kali_downloading_key"));
        auto r = Executor::passthrough("curl -L https://archive.kali.org/archive-key.asc | "
            "gpg --dearmor > /tmp/kali.gpg");
        if (!r.ok()) {
            Logger::error(_("mirror.kali_gpg_failed"));
            return false;
        }

        Executor::shell("install -o root -g root -m 644 /tmp/kali.gpg " + keyring);

        Logger::step(_("mirror.kali_writing_list"));
        std::ofstream ofs(kali_list);
        if (!ofs.is_open()) {
            Logger::error(_f("mirror.kali_write_failed", kali_list));
            return false;
        }
        ofs << "deb [signed-by=" + keyring + "] http://http.kali.org/kali/ kali-rolling main contrib non-free\n";
        ofs << "# deb [signed-by=" + keyring +
                "] http://mirrors.bfsu.edu.cn/kali/ kali-rolling main contrib non-free\n";
        ofs << "# deb [signed-by=" + keyring +
                "] https://mirrors.ustc.edu.cn/kali kali-rolling main non-free contrib\n";
        ofs << "# deb [signed-by=" + keyring +
                "] http://mirrors.bfsu.edu.cn/kali/ kali-last-snapshot main contrib non-free\n";
        ofs.close();

        Logger::step(_("mirror.updating"));
        Executor::passthrough("apt update");
        Executor::shell("apt install -y kali-menu 2>/dev/null || true");

        Logger::ok(_("mirror.kali_switch_ok"));
        return true;
    }

    // ── archlinuxcn 源 ──
    bool MirrorManager::add_archlinuxcn_source() {
        auto r = Executor::shell("grep -q 'archlinuxcn' /etc/pacman.conf");
        if (r.ok()) {
            Logger::info(_("mirror.archlinuxcn_already_added"));
            return true;
        }

        Logger::step(_("mirror.archlinuxcn_adding"));
        Executor::shell(
            "cat >>/etc/pacman.conf <<-'TMOE_EOF'\n"
            "[archlinuxcn]\n"
            "Server = https://mirrors.tuna.tsinghua.edu.cn/archlinuxcn/$arch\n"
            "SigLevel = Never\n"
            "TMOE_EOF");

        Logger::step(_("mirror.archlinuxcn_installing_keyring"));
        Executor::shell("pacman -Syu --noconfirm archlinux-keyring 2>/dev/null || true");
        Executor::shell("pacman -Sy --noconfirm archlinuxcn-keyring 2>/dev/null || true");

        Logger::step(_("mirror.archlinuxcn_installing_paru"));
        if (!Executor::has("paru")) {
            Executor::shell("pacman -S --noconfirm paru 2>/dev/null || true");
        }
        if (!Executor::has("fakeroot")) {
            Executor::shell("pacman -S --noconfirm fakeroot 2>/dev/null || true");
        }

        Logger::ok(_("mirror.archlinuxcn_ok"));
        return true;
    }

    // ── EPEL 源 (RHEL/CentOS) ──
    bool MirrorManager::add_epel_source() {
        Logger::step(_("mirror.epel_installing"));
        auto r = Executor::shell("yes | yum install -y epel-release 2>/dev/null || "
            "yes | dnf install -y epel-release 2>/dev/null || true");

        if (!fs::exists("/etc/yum.repos.d/epel.repo")) {
            Logger::error(_("mirror.epel_failed"));
            return false;
        }

        // 备份
        Executor::shell("cp -pvf /etc/yum.repos.d/epel.repo /etc/yum.repos.d/epel.repo.backup 2>/dev/null || true");
        Executor::shell(
            "cp -pvf /etc/yum.repos.d/epel-testing.repo /etc/yum.repos.d/epel-testing.repo.backup 2>/dev/null || true");

        // 替换为 OpenTUNA 镜像
        Logger::step(_("mirror.epel_replacing"));
        Executor::shell(
            "sed -E -e 's@^(metalink=)@#\\1@g' "
            "-e 's@^#(baseurl)=.*/pub/(epel)@\\1=https://opentuna.cn/\\2@g' "
            "-i /etc/yum.repos.d/epel.repo /etc/yum.repos.d/epel-testing.repo 2>/dev/null || true");

        Logger::ok(_("mirror.epel_ok"));
        return true;
    }

    // ── RPMFusion 源 (Fedora) ──
    bool MirrorManager::add_rpmfusion_source() {
        int fv = detect_fedora_version();
        if (fv <= 0) {
            Logger::error(_("mirror.rpmfusion_no_version"));
            return false;
        }

        Logger::step(_f("mirror.rpmfusion_installing", std::to_string(fv)));

        std::string rpm_free = "https://mirrors.bfsu.edu.cn/rpmfusion/free/fedora/"
                               "rpmfusion-free-release-" + std::to_string(fv) + ".noarch.rpm";
        std::string rpm_nonfree = "https://mirrors.bfsu.edu.cn/rpmfusion/nonfree/fedora/"
                                  "rpmfusion-nonfree-release-" + std::to_string(fv) + ".noarch.rpm";

        Executor::shell("dnf install -y --nogpgcheck " + rpm_free + " " + rpm_nonfree + " 2>/dev/null || "
                        "yum install -y --nogpgcheck " + rpm_free + " " + rpm_nonfree + " 2>/dev/null || true");

        // 替换 baseurl 为 bfsu 镜像
        Executor::shell(
            "for i in $(ls /etc/yum.repos.d/rpmfusion*repo 2>/dev/null | grep -v rawhide); do "
            "  cp -vf ${i} ${i}.backup; "
            "  sed -e 's!^metalink=!#metalink=!g' "
            "      -e 's!^#baseurl=!baseurl=!g' "
            "      -e 's!//download1\\.rpmfusion\\.org/!//mirrors.bfsu.edu.cn/rpmfusion/!g' "
            "      -e 's!http://mirrors\\.bfsu!https://mirrors.bfsu!g' "
            "      -i ${i}; "
            "done");

        Logger::ok(_("mirror.rpmfusion_ok"));
        return true;
    }

    // ── 镜像源 FAQ ──
    void MirrorManager::show_mirror_faq() const {
        // 对应 Bash sources_list_faq (478-484行) — 3行简单建议
        Logger::info(_("mirror.faq_content"));
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "arch")
            Logger::info(_("mirror.faq_trust_hint"));
        Logger::info(_("mirror.faq_change_hint"));
    }

    // ── 顶层 API: switch_to / auto_select / restore_official ──
    bool MirrorManager::switch_to(const std::string &mirror_id) {
        auto mirror_opt = MirrorRegistry::instance().find(mirror_id);
        if (!mirror_opt) {
            Logger::error(_f("mirror.unknown_id", mirror_id));
            return false;
        }

        auto &mirror = *mirror_opt;
        Logger::step(_f("mirror.switch_to", mirror.name, mirror.url));

        // 备份原始源
        ensure_backup();

        // 发行版路由（Debian 家族通过 sub_distro 区分）
        const auto &distro = cfg_.linux_distro;
        const auto &sub = cfg_.sub_distro;
        bool ok = false;

        if (distro == "debian") {
            if (sub == "ubuntu") {
                ok = write_ubuntu_sources(mirror);
            } else if (sub == "kali") {
                ok = write_kali_sources(mirror);
            } else {
                ok = write_debian_sources(mirror);
            }
        } else if (distro == "arch") {
            ok = write_arch_sources(mirror);
        } else if (distro == "alpine") {
            ok = write_alpine_sources(mirror);
        } else if (distro == "manjaro") {
            ok = write_manjaro_sources(mirror);
        } else if (distro == "redhat") {
            ok = write_redhat_sources(mirror);
        } else if (distro == "solus") {
            ok = write_solus_sources(mirror);
        } else {
            Logger::error(_f("mirror.unsupported_distro", distro));
            return false;
        }

        if (!ok) return false;

        // 如已安装 ca-certificates 则自动升级 HTTP → HTTPS
        http_to_https_if_ca_available();

        // 刷新软件包数据库
        bool update_ok = run_update();
        if (!update_ok) {
            Logger::warn(_("mirror.update_failed_hint"));
            Logger::info(_("mirror.update_troubleshoot"));
        } else {
            run_dist_upgrade();
        }

        if (update_ok) {
            Logger::ok(_("mirror.switch_ok"));
        }
        return true;
    }

    bool MirrorManager::auto_select() {
        Logger::step(_("mirror.auto_select_start"));

        struct PingResult {
            std::string id;
            std::string name;
            double avg_ms;
        };
        std::vector<PingResult> results;

        auto all_mirrors = MirrorRegistry::instance().all();

        // 过滤不兼容当前发行版的镜像站
        // ftp.*.debian.org / deb.debian.org 只托管 Debian 包，不适用于 Ubuntu/Kali
        std::vector<MirrorEntry> compatible_mirrors;
        bool needs_distro_filter = (cfg_.sub_distro == "ubuntu" || cfg_.sub_distro == "kali");
        Logger::info(_f("mirror.auto_select_skip_debian", cfg_.sub_distro));
        for (const auto &m: all_mirrors) {
            if (needs_distro_filter && m.url.find(".debian.org") != std::string::npos) {
                continue;
            }
            compatible_mirrors.push_back(m);
        }

        Logger::info(_f("mirror.auto_select_resolving_dns", std::to_string(compatible_mirrors.size()),
                        std::to_string(all_mirrors.size())));

        struct ResolvedMirror {
            std::string id;
            std::string name;
            std::string ip;
        };
        std::vector<ResolvedMirror> resolved;

        for (const auto &m: compatible_mirrors) {
            std::string ip = resolve_hostname(m.url);
            if (!ip.empty()) {
                resolved.push_back({m.id, m.name, ip});
                // Logger::debug("  " + m.url + " → " + ip);
            } else {
                Logger::warn(_f("mirror.auto_select_dns_failed_single", m.url));
            }
        }

        if (resolved.empty()) {
            Logger::warn(_("mirror.auto_select_dns_failed"));
            return false;
        }

        // 第二步: ping 已解析的 IP 地址（零 DNS 开销）
        for (const auto &rm: resolved) {
            auto r = Executor::shell(
                "ping -c 3 -W 2 " + rm.ip +
                " 2>/dev/null | grep 'rtt min/avg/max' | cut -d/ -f5");
            if (r.ok() && !r.stdout_data.empty()) {
                try {
                    double avg = std::stod(r.stdout_data);
                    results.push_back({rm.id, rm.name, avg});
                    std::string color = (avg < 30) ? "🟢" : (avg < 80) ? "🟡" : "🔴";
                    Logger::info("  " + color + " " + rm.name + ": " +
                                 std::to_string(avg).substr(0, 6) + " ms");
                } catch (...) {
                    Logger::warn(_f("mirror.ping_result_parse_failed", rm.name));
                }
            } else {
                Logger::warn(_f("mirror.ping_unreachable", rm.name, ""));
            }
        }

        if (results.empty()) {
            Logger::warn(_("mirror.auto_select_no_ping"));
            return false;
        }

        // 按延迟升序排序
        std::sort(results.begin(), results.end(),
                  [](const PingResult &a, const PingResult &b) { return a.avg_ms < b.avg_ms; });

        Logger::info("───────────────��───────────────────────────");
        Logger::info(_("mirror.auto_select_top5"));
        for (size_t i = 0; i < std::min(results.size(), size_t(5)); ++i) {
            std::string medal = (i == 0) ? "🥇" : (i == 1) ? "🥈" : (i == 2) ? "🥉" : "  ";
            Logger::info("  " + medal + " " + _f("mirror.ping_rank_entry", std::to_string(i + 1), results[i].name,
                                                 std::to_string(results[i].avg_ms).substr(0, 6)));
        }

        auto &best = results[0];
        Logger::ok(_f("mirror.auto_select_best", best.name, std::to_string(best.avg_ms).substr(0, 6)));
        return switch_to(best.id);
    }

    bool MirrorManager::restore_official() {
        auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
        if (!compat) {
            Logger::error(_("mirror.no_backup_config_fatal"));
            return false;
        }

        std::string backup_path = compat->backup_path;
        if (!backup_path.empty() && backup_path[0] == '~') {
            backup_path = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + backup_path.substr(1);
        }

        if (!fs::exists(backup_path)) {
            Logger::error(_f("mirror.restore_no_backup", backup_path));
            return false;
        }

        Logger::step(_("mirror.restoring_backup"));

        if (cfg_.linux_distro == "redhat") {
            Executor::shell("tar -Ppzxvf " + backup_path);
            return true;
        }

        // 非 RedHat: 普通文件拷贝
        std::string source = compat->source_file;
        auto r = Executor::shell("cp -pfv " + backup_path + " " + source);
        Logger::ok_or_fail(r.ok(), "还原 " + source);

        // 显示新旧对比
        Executor::shell("diff " + backup_path + " " + source + " -y --color 2>/dev/null || true");

        // Arch: 额外还原 pacman.conf
        if (cfg_.linux_distro == "arch" && !compat->backup_conf.empty()) {
            std::string bc = compat->backup_conf;
            if (!bc.empty() && bc[0] == '~') {
                bc = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + bc.substr(1);
            }
            if (fs::exists(bc)) {
                Executor::shell("cp -pf " + bc + " " + compat->source_conf);
            }
        }

        run_update();
        Logger::ok(_("mirror.restore_complete"));
        return true;
    }

    // ── 辅助操作 ──
    bool MirrorManager::edit_manually() {
        auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
        std::string file;

        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
            // 优先使用 apt edit-sources（交互式，需透传终端）
            if (Executor::has("apt")) {
                Executor::passthrough("apt edit-sources 2>/dev/null || nano /etc/apt/sources.list");
            } else {
                Executor::passthrough("nano /etc/apt/sources.list");
            }
        } else if (cfg_.linux_distro == "arch") {
            Executor::passthrough("nano /etc/pacman.d/mirrorlist /etc/pacman.conf");
        } else if (cfg_.linux_distro == "redhat" && compat) {
            Executor::passthrough("nano " + compat->source_dir + "/*repo");
        } else if (compat && !compat->source_file.empty()) {
            Executor::passthrough("nano " + compat->source_file);
        } else {
            Logger::error(_("mirror.edit_no_target"));
            return false;
        }
        return true;
    }

    bool MirrorManager::toggle_http_https(bool use_https) {
        auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);

        if (cfg_.linux_distro == "redhat" && compat) {
            if (use_https) {
                Executor::shell("sed -i 's@http://@https://@g' " + compat->source_dir + "/*repo");
            } else {
                Executor::shell("sed -i 's@https://@http://@g' " + compat->source_dir + "/*repo");
            }
        } else if (compat && !compat->source_file.empty()) {
            if (use_https) {
                Executor::shell("sed -i 's@http://@https://@g' " + compat->source_file);
            } else {
                Executor::shell("sed -i 's@https://@http://@g' " + compat->source_file);
            }
        } else {
            Logger::error(_("mirror.no_source_file"));
            return false;
        }

        Logger::ok(use_https ? _("mirror.switched_to_https") : _("mirror.switched_to_http"));
        run_update();
        return true;
    }

    bool MirrorManager::clean_sources_list() {
        auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
        if (!compat || compat->source_file.empty()) {
            Logger::error(_("mirror.no_source_file"));
            return false;
        }

        Logger::step(_("mirror.cleaning"));

        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
            Executor::shell("sed -i '/^#/d' " + compat->source_file);
        } else if (cfg_.linux_distro == "arch") {
            Executor::shell("sed -i '/^#Server.*=/d' " + compat->source_file);
        } else if (cfg_.linux_distro == "alpine") {
            Executor::shell("sed -i '/^#.*http/d' " + compat->source_file);
        }

        // 去重 (sort -u)
        Executor::shell("sort -u " + compat->source_file + " -o " + compat->source_file);

        run_update();
        Logger::ok(_("mirror.clean_complete"));
        return true;
    }

    bool MirrorManager::trust_sources(bool trust) {
        if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
            if (trust) {
                Logger::warn(_("mirror.trust_warning_detail"));
                Executor::shell("sed -i 's@^deb.*http@deb [trusted=yes] http@g' /etc/apt/sources.list");
            } else {
                Executor::shell("sed -i 's@^deb \\[trusted=yes\\] http@deb http@g' /etc/apt/sources.list");
            }
        } else if (cfg_.linux_distro == "arch") {
            if (trust) {
                Executor::shell("sed -i 's@^#SigLevel.*@SigLevel = Never@' /etc/pacman.conf");
            } else {
                Executor::shell("sed -i 's@^SigLevel = Never@#SigLevel = Optional TrustAll@' /etc/pacman.conf");
            }
        } else {
            Logger::error(_("mirror.trust_unsupported"));
            return false;
        }

        Logger::ok(trust ? _("mirror.trust_enabled") : _("mirror.trust_disabled"));
        run_update();
        return true;
    }
} // namespace tmoe::domain
