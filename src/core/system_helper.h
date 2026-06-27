#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

namespace tmoe {

class SystemHelper {
public:
    // ---------- 文件 I/O ----------
    static bool write_file(const fs::path& path, std::string_view content);
    static bool append_file(const fs::path& path, std::string_view content);
    static std::string read_file(const fs::path& path);

    // ---------- 包安装 ----------
    static bool install_packages(const std::vector<std::string>& packages, const std::string& install_command);

    // ---------- 用户路径 ----------
    /** 获取真实用户 home 路径（兼容 sudo 环境）。 */
    static std::string user_home();

    // ---------- 权限修复 ----------
    /** sudo 提权操作后统一修复用户 home 目录归属。 */
    static void fix_user_home_ownership();
};

} // namespace tmoe::domain
