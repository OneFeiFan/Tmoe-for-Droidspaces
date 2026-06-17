#include "mirrors.h"

namespace fs = std::filesystem;

namespace tmoe::domain {

MirrorManager::MirrorManager(const TmoeConfig& cfg) : cfg_(cfg) {}

std::vector<MirrorEntry> MirrorManager::available() const {
    return MirrorRegistry::instance().all();
}

std::vector<MirrorEntry> MirrorManager::by_category(const std::string& cat) const {
    return MirrorRegistry::instance().by_category(cat);
}

/** 检测当前发行版的版本代号。
 *  读取 /etc/os-release 中的 VERSION_CODENAME；失败时回退到 PRETTY_NAME。
 */
std::string MirrorManager::detect_version_codename() const {
    const auto& distro = cfg_.linux_distro;

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
            Logger::warn("VERSION_CODENAME 不可用，回退到检测到的版本名: " + s);
            return s;
        }
        Logger::error("无法检测发行版版本号，回退到 sid");
        return "sid";
    }

    // Alpine: 从 PRETTY_NAME 提取版本号
    if (distro == "alpine") {
        auto r = Executor::shell(
            "grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"' | awk '{print $NF}'");
        if (r.ok() && !r.stdout_data.empty()) {
            auto s = r.stdout_data;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            return s;  // 例如 "v3.18"
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
        Logger::debug("当前发行版 " + cfg_.linux_distro + " 无备份路径配置，跳过备份");
        return true;
    }

    // 一次性快照: 备份已存在则不覆盖
    if (fs::exists(backup_path)) {
        Logger::debug("备份文件已存在，跳过: " + backup_path);
        return true;
    }

    // RedHat: 打包整个 yum.repos.d 目录
    if (cfg_.linux_distro == "redhat") {
        Logger::step("正在创建 RedHat yum.repos.d 备份...");
        auto r = Executor::shell("mkdir -p \"$(dirname " + backup_path + ")\" && "
                                 "tar -Ppzcvf " + backup_path + " " + compat->source_dir);
        Logger::ok_or_fail(r.ok(), "备份 yum.repos.d");
        return r.ok();
    }

    // 普通文件拷贝
    Logger::step("正在备份原始软件源文件...");
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
    Logger::step("更新软件包数据库...");
    if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
        return Executor::shell("apt update").ok();
    }
    if (cfg_.linux_distro == "arch") {
        return Executor::shell("pacman -Syyu --noconfirm").ok();
    }
    if (cfg_.linux_distro == "alpine") {
        return Executor::shell("apk update").ok();
    }
    return true;
}

bool MirrorManager::run_dist_upgrade() const {
    if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
        return Executor::shell("apt dist-upgrade -y").ok();
    }
    if (cfg_.linux_distro == "arch") {
        return Executor::shell("pacman -Su --noconfirm").ok();
    }
    if (cfg_.linux_distro == "alpine") {
        return Executor::shell("apk upgrade").ok();
    }
    return true;
}

bool MirrorManager::http_to_https_if_ca_available() const {
    if (cfg_.linux_distro != "debian" && cfg_.linux_distro != "ubuntu" && cfg_.linux_distro != "kali") {
        return true;
    }
    if (!fs::exists("/usr/sbin/update-ca-certificates")) {
        Logger::debug("未安装 ca-certificates，保持 http 源");
        return true;
    }
    Logger::step("检测到 ca-certificates，将 http 源替换为 https...");
    Executor::shell("sed -i 's@http://@https://@g' /etc/apt/sources.list");
    Executor::shell("sed -i 's@https://security@http://security@g' /etc/apt/sources.list");
    return true;
}

std::string MirrorManager::resolve_hostname(const std::string& url) const {
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

    return "";  // 所有方法均解析失败
}

// ── 各发行版源写入器（对齐原版 Bash heredoc 模板）──
bool MirrorManager::write_debian_sources(const MirrorEntry& mirror) {
    std::string codename = detect_version_codename();
    std::string url = mirror.url;

    Logger::info("检测到 Debian " + codename + " 系统，正在切换到 " + mirror.name);

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

bool MirrorManager::write_ubuntu_sources(const MirrorEntry& mirror) {
    std::string codename = detect_version_codename();
    std::string url = mirror.url;

    Logger::info("检测到 Ubuntu " + codename + " 系统，正在切换到 " + mirror.name);

    // 注释旧行 → 写入新模板
    Executor::shell("sed -i 's/^deb/# &/g' /etc/apt/sources.list");

    Executor::shell("cat >>/etc/apt/sources.list <<-TMOE_EOF\n"
                    "deb http://" + url + "/ubuntu/ " + codename + " main restricted universe multiverse\n"
                    "deb http://" + url + "/ubuntu/ " + codename + "-updates main restricted universe multiverse\n"
                    "deb http://" + url + "/ubuntu/ " + codename + "-backports main restricted universe multiverse\n"
                    "deb http://" + url + "/ubuntu/ " + codename + "-security main restricted universe multiverse\n"
                    "TMOE_EOF");

    // ARM: 自动切换到 ubuntu-ports
    if (cfg_.arch != "amd64" && cfg_.arch != "i386" && cfg_.arch != "x86_64") {
        Logger::step("非 x86 架构，自动切换到 ubuntu-ports...");
        Executor::shell("sed -i 's:/ubuntu:/ubuntu-ports:g' /etc/apt/sources.list");
    }

    return true;
}

bool MirrorManager::write_kali_sources(const MirrorEntry& mirror) {
    std::string url = mirror.url;
    Logger::info("检测到 Kali 系统，正在切换到 " + mirror.name);

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

bool MirrorManager::write_arch_sources(const MirrorEntry& mirror) {
    std::string url = mirror.url;
    Logger::info("检测到 Arch Linux 系统，正在切换到 " + mirror.name);

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

bool MirrorManager::write_alpine_sources(const MirrorEntry& mirror) {
    std::string version = detect_version_codename();
    std::string url = mirror.url;

    Logger::info("检测到 Alpine " + version + " 系统，正在切换到 " + mirror.name);

    // Alpine: 注释所有旧 http 行
    Executor::shell("cd /etc/apk/ && sed -i 's@^http@#&@g' repositories");

    // 写入新仓库
    Executor::shell("cat >>/etc/apk/repositories <<-TMOE_EOF\n"
                    "http://" + url + "/alpine/" + version + "/main\n"
                    "http://" + url + "/alpine/" + version + "/community\n"
                    "TMOE_EOF");

    return true;
}

// ── 顶层 API: switch_to / auto_select / restore_official ──
bool MirrorManager::switch_to(const std::string& mirror_id) {
    auto mirror_opt = MirrorRegistry::instance().find(mirror_id);
    if (!mirror_opt) {
        Logger::error("未知的镜像源 ID: " + mirror_id);
        return false;
    }

    auto& mirror = *mirror_opt;
    Logger::step("切换镜像源: " + mirror.name + " (" + mirror.url + ")");

    // 备份原始源
    ensure_backup();

    // 发行版路由
    const auto& distro = cfg_.linux_distro;
    bool ok = false;

    if (distro == "debian") {
        ok = write_debian_sources(mirror);
    } else if (distro == "ubuntu") {
        ok = write_ubuntu_sources(mirror);
    } else if (distro == "kali") {
        ok = write_kali_sources(mirror);
    } else if (distro == "arch") {
        ok = write_arch_sources(mirror);
    } else if (distro == "alpine") {
        ok = write_alpine_sources(mirror);
    } else {
        Logger::error("不支持的发行版: " + distro + "，镜像源切换暂未实现");
        return false;
    }

    if (!ok) return false;

    // 如已安装 ca-certificates 则自动升级 HTTP → HTTPS
    http_to_https_if_ca_available();

    // 刷新软件包数据库
    run_update();
    run_dist_upgrade();

    Logger::ok("镜像源已切换完成！");
    return true;
}

bool MirrorManager::auto_select() {
    Logger::step("自动选择最快镜像源（先解析DNS，再ping测速）...");

    struct PingResult {
        std::string id;
        std::string name;
        double avg_ms;
    };
    std::vector<PingResult> results;

    auto cn_mirrors = MirrorRegistry::instance().by_region("cn");

    // 第一步: 批量 DNS 预解析，避免每个 ping 各自等 DNS
    struct ResolvedMirror {
        std::string id;
        std::string name;
        std::string ip;
    };
    std::vector<ResolvedMirror> resolved;

    Logger::info("正在解析 DNS...");
    for (const auto& m : cn_mirrors) {
        std::string ip = resolve_hostname(m.url);
        if (!ip.empty()) {
            resolved.push_back({m.id, m.name, ip});
            Logger::debug("  " + m.url + " → " + ip);
        } else {
            Logger::warn("  DNS 解析失败: " + m.url + "，跳过");
        }
    }

    if (resolved.empty()) {
        Logger::warn("所有镜像站 DNS 均解析失败，无法测速");
        return false;
    }

    // 第二步: ping 已解析的 IP 地址（零 DNS 开销）
    for (const auto& rm : resolved) {
        Logger::info("  ping " + rm.name + " (" + rm.ip + ")...");
        auto r = Executor::shell(
            "ping -c 3 -W 2 " + rm.ip +
            " 2>/dev/null | grep 'rtt min/avg/max' | cut -d/ -f5");
        if (r.ok() && !r.stdout_data.empty()) {
            try {
                double avg = std::stod(r.stdout_data);
                results.push_back({rm.id, rm.name, avg});
                Logger::info("    " + rm.name + ": " + std::to_string(avg) + " ms");
            } catch (...) {
                Logger::warn("    " + rm.name + ": ping 结果无法解析");
            }
        } else {
            Logger::warn("    " + rm.name + ": 无法 ping 通");
        }
    }

    if (results.empty()) {
        Logger::warn("所有镜像站均无法 ping 通，保持当前源不变");
        return false;
    }

    // 按延迟升序排序
    std::sort(results.begin(), results.end(),
              [](const PingResult& a, const PingResult& b) { return a.avg_ms < b.avg_ms; });

    auto& best = results[0];
    Logger::ok("最快镜像站: " + best.name + " (" + std::to_string(best.avg_ms) + " ms)");
    return switch_to(best.id);
}

bool MirrorManager::restore_official() {
    auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
    if (!compat) {
        Logger::error("当前发行版无备份路径配置");
        return false;
    }

    std::string backup_path = compat->backup_path;
    if (!backup_path.empty() && backup_path[0] == '~') {
        backup_path = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + backup_path.substr(1);
    }

    if (!fs::exists(backup_path)) {
        Logger::error("备份文件不存在: " + backup_path + "，无法还原");
        return false;
    }

    Logger::step("正在从备份还原软件源...");

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
    Logger::ok("软件源已还原至备份状态");
    return true;
}

// ── 辅助操作 ──
bool MirrorManager::edit_manually() {
    auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
    std::string file;

    if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
        // 优先使用 apt edit-sources
        if (Executor::has("apt")) {
            Executor::shell("apt edit-sources 2>/dev/null || nano /etc/apt/sources.list");
        } else {
            Executor::shell("nano /etc/apt/sources.list");
        }
    } else if (cfg_.linux_distro == "arch") {
        Executor::shell("nano /etc/pacman.d/mirrorlist /etc/pacman.conf");
    } else if (cfg_.linux_distro == "redhat" && compat) {
        Executor::shell("nano " + compat->source_dir + "/*repo");
    } else if (compat && !compat->source_file.empty()) {
        Executor::shell("nano " + compat->source_file);
    } else {
        Logger::error("无法确定编辑目标文件");
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
        Logger::error("无法确定源文件路径");
        return false;
    }

    Logger::ok(use_https ? "已切换为 HTTPS 源" : "已切换为 HTTP 源");
    run_update();
    return true;
}

bool MirrorManager::clean_sources_list() {
    auto compat = MirrorRegistry::instance().compat_for(cfg_.linux_distro);
    if (!compat || compat->source_file.empty()) {
        Logger::error("无法确定源文件路径");
        return false;
    }

    Logger::step("正在清理注释行并去重...");

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
    Logger::ok("清理完成，刷新软件包数据库");
    return true;
}

bool MirrorManager::trust_sources(bool trust) {
    if (cfg_.linux_distro == "debian" || cfg_.linux_distro == "ubuntu" || cfg_.linux_distro == "kali") {
        if (trust) {
            Logger::warn("强制信任软件源可能有安全风险！");
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
        Logger::error("当前发行版不支持强制信任操作");
        return false;
    }

    Logger::ok(trust ? "已强制信任软件源" : "已取消强制信任");
    run_update();
    return true;
}

} // namespace tmoe::domain
