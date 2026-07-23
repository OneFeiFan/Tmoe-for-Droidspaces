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
        static bool write_file(const fs::path &path, std::string_view content);

        static bool append_file(const fs::path &path, std::string_view content);

        static std::string read_file(const fs::path &path);

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
                                             const std::string &extract_to = "/usr/share/backgrounds");

        /** git clone 浅克隆 + tar 解压到目标目录。 */
        static bool git_clone_and_extract(const std::string &git_url,
                                          const std::string &branch,
                                          const std::string &archive_name,
                                          const std::string &tmp_dir,
                                          const std::string &extract_to);

        // ---------- 用户路径 ----------
        /** 获取真实用户 home 路径（兼容 sudo 环境）。 */
        static std::string user_home();

        /** 获取用户图片目录：$XDG_PICTURES_DIR → ~/.config/user-dirs.dirs → ~/Pictures */
        static std::string user_pictures_dir();


        /** 从 HTML 索引页中查找匹配正则的最新包文件名。 */
        static std::string find_latest_href(const std::string &html, const std::string &pkg_pattern);

    private:
        // extract_archive 命令片段构建器（拆分 shell 脚本便于调试和测试）
        static std::string build_tar_pipe_cmd(const std::string &safe_archive, bool has_pv);

        static std::string build_deb_extract_cmd(const std::string &safe_archive);

        static std::string build_smart_transfer_cmd(const std::string &safe_dest);
    };

} // namespace tmoe
