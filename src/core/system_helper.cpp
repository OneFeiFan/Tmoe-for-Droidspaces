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

} // namespace tmoe::domain
