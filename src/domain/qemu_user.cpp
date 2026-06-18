#include "domain/runtime.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/i18n.h"
#include <filesystem>
#include <cstdlib>
#include <unordered_map>

namespace fs = std::filesystem;
using tmoe::Executor;

namespace tmoe::domain {

// ── QemuUserRuntime: qemu-user-static 管理 ──

bool QemuUserRuntime::install() {
    const auto &c = config_;
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";

    // 此函数逻辑对应 Bash 版 install_qemu_user_static()
    // 简化版: 检查已有版本，需要时下载
    std::string ver_file = tmoe_linux_dir + "/lib/usr/bin/version.txt";
    Logger::step(_("qemu_user.checking"));

    // 构建完整 QEMU 架构列表
    // 对应 Bash 版: amd64→x86_64, arm64→aarch64, ppc64el→ppc64le
    Logger::step(_("qemu_user.install_hint"));
    Logger::info("Debian/Ubuntu: apt install qemu-user-static");
    Logger::info("Arch Linux: pacman -S qemu-user-static");
    Logger::info("Termux: pkg install qemu-user-{arch}");

    // 检查常用架构二进制
    const std::vector<std::string> arches = {"x86_64", "i386", "aarch64", "arm", "armeb",
                                              "mipsel", "mips64el", "ppc64le", "s390x", "riscv64"};
    std::string prefix = c.qemu_32_enabled ? c.qemu_user_32_prefix : c.qemu_user_prefix;
    // 替换变量
    size_t pos;
    while ((pos = prefix.find("${TMOE_LINUX_DIR}")) != std::string::npos)
        prefix.replace(pos, 18, tmoe_linux_dir);

    int found = 0;
    for (const auto &arch : arches) {
        std::string path = prefix + "/qemu-" + arch + "-static";
        if (fs::exists(path)) {
            found++;
        }
    }

    if (found > 0) {
        Logger::ok(_f("qemu_user.installed_count", std::to_string(found)));
        return true;
    }
    Logger::warn(_("qemu_user.not_detected"));
    return false;
}

bool QemuUserRuntime::remove() {
    Logger::step(_("qemu_user.removing"));
    // 对应 Bash 版 remove_qemu_user_static(): 删除二进制文件和版本文件
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";
    std::string prefix = tmoe_linux_dir + "/lib/usr/bin";

    if (fs::exists(prefix)) {
        Executor::shell("rm -f " + prefix + "/qemu-*-static");
        Executor::shell("rm -f " + prefix + "/version.txt");
        Logger::ok(_("qemu_user.removed"));
    }
    return true;
}

bool QemuUserRuntime::check_version(std::string &version_out) const {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    std::string tmoe_linux_dir = home + "/.local/share/tmoe-linux";
    std::string ver_file = tmoe_linux_dir + "/lib/usr/bin/version.txt";

    if (fs::exists(ver_file)) {
        auto r = Executor::shell("cat " + ver_file);
        if (r.ok()) {
            version_out = r.stdout_data;
            // trim
            if (!version_out.empty() && version_out.back() == '\n') version_out.pop_back();
            return true;
        }
    }
    version_out = "unknown";
    return false;
}

std::string QemuUserRuntime::detect_qemu_bin(const std::string &container_arch,
                                              const std::string &host_arch) const {
    // 同架构则无需 QEMU
    if (container_arch == host_arch) return "";

    // 特例
    if (container_arch == "i386" && (host_arch == "amd64" || host_arch == "i386")) return "";
    if (container_arch == "armhf" && (host_arch == "arm64" || host_arch == "armhf")) return "";
    if (container_arch == "armel" && (host_arch == "arm64" || host_arch == "armhf" || host_arch == "armel")) return "";

    // 架构映射
    std::unordered_map<std::string, std::string> arch_map = {
        {"amd64", "x86_64"}, {"i386", "i386"}, {"arm64", "aarch64"},
        {"armhf", "arm"}, {"armel", "armeb"}, {"mipsel", "mipsel"},
        {"mips64el", "mips64el"}, {"ppc64el", "ppc64le"}, {"s390x", "s390x"},
        {"riscv64", "riscv64"}
    };

    auto it = arch_map.find(container_arch);
    if (it == arch_map.end()) return "";

    const auto &c = config_;
    std::string qemu_arch = it->second;
    std::string qemu_path = c.qemu_32_enabled ? c.qemu_user_32_prefix : c.qemu_user_prefix;
    if (c.qemu_user_static)
        return qemu_path + "/qemu-" + qemu_arch + "-static";
    return "qemu-" + qemu_arch;
}

} // namespace tmoe::domain
