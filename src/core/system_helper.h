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
};

} // namespace tmoe::domain
