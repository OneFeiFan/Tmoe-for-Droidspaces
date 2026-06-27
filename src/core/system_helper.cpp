#include "system_helper.h"
#include "core/logger.h"
#include "core/executor.h"
#include "core/i18n.h"
#include <fstream>
#include <sstream>

namespace tmoe {

// ---------- 文件 I/O ----------

bool SystemHelper::write_file(const fs::path &path, std::string_view content) {
    try {
        fs::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs << content;
        return ofs.good();
    } catch (const std::exception &e) {
        Logger::error(std::string("写入文件失败: ") + path.string() + " — " + e.what());
        return false;
    }
}

bool SystemHelper::append_file(const fs::path &path, std::string_view content) {
    try {
        fs::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::app);
        if (!ofs.is_open()) return false;
        ofs << content;
        return ofs.good();
    } catch (const std::exception &e) {
        Logger::error(std::string("追加文件失败: ") + path.string() + " — " + e.what());
        return false;
    }
}

std::string SystemHelper::read_file(const fs::path &path) {
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return {};
        std::ostringstream oss;
        oss << ifs.rdbuf();
        return oss.str();
    } catch (...) {
        return {};
    }
}

// ---------- 包安装 ----------

bool SystemHelper::install_packages(const std::vector<std::string> &packages,
                                    const std::string &install_command) {
    std::ostringstream oss;
    for (const auto &pkg : packages) oss << pkg << " ";
    std::string cmd = install_command + " " + oss.str();
    Logger::step(_f("gui.installing_pkgs", oss.str()));
    return Executor::passthrough(cmd).ok();
}

// ---------- 用户路径 ----------

std::string SystemHelper::user_home() {
    const char* sudo_user = std::getenv("SUDO_USER");
    if (sudo_user && sudo_user[0]) {
        auto r = Executor::shell(
            std::string("getent passwd ") + sudo_user + " | cut -d: -f6 2>/dev/null");
        if (r.ok() && !r.stdout_data.empty()) {
            std::string h = r.stdout_data;
            while (!h.empty() && (h.back() == '\n' || h.back() == '\r')) h.pop_back();
            if (!h.empty()) return h;
        }
    }
    const char* home = std::getenv("HOME");
    return home ? home : "/root";
}

// ---------- 权限修复 ----------

void SystemHelper::fix_user_home_ownership() {
    // 仅 root 且存在 $SUDO_USER 时修复（非 sudo 环境无需此操作）
    const char* sudo_user = std::getenv("SUDO_USER");
    if (!sudo_user || !sudo_user[0]) return;
    std::string home = user_home();

    // 预建常用目录
    Executor::shell("mkdir -p " + home +
                    "/.local/share " + home + "/.config/autostart " +
                    home + "/.cache/sessions 2>/dev/null || true");

    // 一次性修复所有用户目录归属
    std::string user(sudo_user);
    for (const auto* dir : {
        "/.vnc","/.config","/.cache","/.local","/.dbus",
        "/.Xauthority","/.ICEauthority",
        "/.profile","/.bashrc","/.zshrc",
        "/.zsh_history","/.bash_history",
        "/.aria2","/.conky","/Pictures","/github"
    }) {
        Executor::passthrough(
            "chown -R " + user + ":" + user + " " + home + dir + " 2>/dev/null || true");
    }
}

} // namespace tmoe
