#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <string_view>
#include <regex>

namespace fs = std::filesystem;

namespace tmoe {

class SystemHelper {
public:
    // ---------- 文件 I/O ----------
    static bool write_file(const fs::path& path, std::string_view content);
    static bool append_file(const fs::path& path, std::string_view content);
    static std::string read_file(const fs::path& path);

    // ---------- 下载 / 解压 ----------
    /** 下载单个文件（curl→wget→aria2c 三重回退），返回是否成功。 */
    static bool download_file(const std::string &url, const std::string &dest_path);

    /** 下载 URL 到临时文件并返回内容字符串。 */
    static std::string http_get_cached(const std::string &url, const std::string &cache_name);

    /** 解压归档到目标目录（tar/deb/zip 自动识别，tar 保留原始权限）。 */
    static void extract_archive(const std::string &archive_path, const std::string &dest = "/");

    /** 从仓库 HTML 索引抓取最新匹配的包 → 下载 → 解压到目标目录。 */
    static bool fetch_latest_and_extract(const std::string &repo_url,
                                          const std::string &pkg_pattern,
                                          const std::string &tmp_prefix,
                                          const std::string &extract_to = "/");

    /** git clone 浅克隆 + tar 解压到目标目录。 */
    static bool git_clone_and_extract(const std::string &git_url,
                                       const std::string &branch,
                                       const std::string &archive_name,
                                       const std::string &tmp_dir,
                                       const std::string &extract_to);

    // ---------- 包安装 ----------
    static bool install_packages(const std::vector<std::string>& packages, const std::string& install_command);

    // ---------- 用户路径 ----------
    /** 获取真实用户 home 路径（兼容 sudo 环境）。 */
    static std::string user_home();

    // ---------- 权限修复 ----------
    /** sudo 提权操作后统一修复用户 home 目录归属。 */
    static void fix_user_home_ownership();

    /** 从 HTML 索引页中查找匹配正则的最新包文件名。 */
    static std::string find_latest_href(const std::string &html, const std::string &pkg_pattern);
};

} // namespace tmoe
