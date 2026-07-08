#include "system_helper.h"
#include "core/logger.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"

#include <fstream>
#include <sstream>
#include <random>
#include <regex>
#include <system_error>

#ifdef _WIN32
// === Windows 编译环境的 Polyfill (垫片) ===
#include <process.h>
inline int getpid() { return _getpid(); }
inline int getuid() { return 0; } // 伪造 UID 以骗过编译器
#else
// === Linux / WSL 原生环境 ===
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace tmoe {

namespace {
    namespace fs = std::filesystem;

    // 内部防注入转义（针对必须进 Shell 的路径）
    std::string safe_escape(std::string_view arg) {
        if (arg.empty()) return "''";
        std::string escaped = "'";
        for (char c : arg) {
            if (c == '\'') escaped += "'\\''";
            else escaped += c;
        }
        escaped += "'";
        return escaped;
    }

    // 生成高离散本地临时文件，防御 Symlink 攻击
    fs::path generate_temp_path(const std::string& prefix = "tmoe") {
        auto temp_dir = fs::temp_directory_path();
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
        std::stringstream ss;
        ss << "." << prefix << "_" << getpid() << "_" << std::hex << dis(gen) << ".tmp";
        return temp_dir / ss.str();
    }

    bool run_sudo(const std::string& cmd) {
        return Executor::shell("sudo " + cmd).ok();
    }

    // 安全的目录级联创建
    bool ensure_parent_dir(const fs::path& parent) {
        if (parent.empty() || fs::exists(parent)) return true;
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (!ec) return true;
        if (ec.value() == static_cast<int>(std::errc::permission_denied)) {
            return run_sudo("mkdir -p " + safe_escape(parent.string()));
        }
        return false;
    }

    // 自适应写入原子操作核心
    bool adaptive_write_core(const fs::path &path, std::string_view content, bool append) {
        fs::path tmp_path = generate_temp_path("write");
        struct TempRemover { fs::path p; ~TempRemover() { std::error_code e; fs::remove(p, e); } } rm{tmp_path};

        try {
            if (append && fs::exists(path)) {
                std::error_code ec;
                fs::copy_file(path, tmp_path, fs::copy_options::overwrite_existing, ec);
                if (ec && ec.value() == static_cast<int>(std::errc::permission_denied)) {
                    if (!run_sudo("cp -f " + safe_escape(path.string()) + " " + safe_escape(tmp_path.string()))) {
                        return false;
                    }
                    run_sudo("chown " + std::to_string(getuid()) + " " + safe_escape(tmp_path.string()));
                }
            }

            auto mode = append ? (std::ios::app | std::ios::out) : (std::ios::trunc | std::ios::out);
            std::ofstream ofs(tmp_path, mode);
            if (!ofs.is_open()) return false;
            ofs << content;
            ofs.flush();
            if (!ofs.good()) return false;
            ofs.close();

            if (!ensure_parent_dir(path.parent_path())) return false;

            std::error_code move_ec;
            if (fs::exists(path)) {
                fs::copy_file(tmp_path, path, fs::copy_options::overwrite_existing, move_ec);
                if (!move_ec) fs::remove(tmp_path);
            } else {
                fs::rename(tmp_path, path, move_ec);
            }

            if (move_ec && move_ec.value() == static_cast<int>(std::errc::permission_denied)) {
                return run_sudo("cp -f " + safe_escape(tmp_path.string()) + " " + safe_escape(path.string()));
            }
            return move_ec.value() == 0;
        } catch (...) {
            return false;
        }
    }
}

// ---------- 文件 I/O ----------

bool SystemHelper::write_file(const fs::path &path, std::string_view content) {
    bool ok = adaptive_write_core(path, content, false);
    if (!ok) Logger::error("Write file failed: " + path.string());
    return ok;
}

bool SystemHelper::append_file(const fs::path &path, std::string_view content) {
    bool ok = adaptive_write_core(path, content, true);
    if (!ok) Logger::error("Append file failed: " + path.string());
    return ok;
}

std::string SystemHelper::read_file(const fs::path &path) {
    // 保持旧版接口签名与异常吞噬机制，确保兼容上层
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
    for (const auto &pkg : packages) {
        // 对包名强制转义，杜绝 "pkg; rm -rf /" 的注入攻击
        oss << safe_escape(pkg) << " ";
    }
#ifndef _WIN32
    bool need_sudo = (geteuid() != 0);
#else
    bool need_sudo = false;
#endif
    std::string cmd = (need_sudo ? "sudo " : "") + install_command + " " + oss.str();
    Logger::step(_f("gui.installing_pkgs", oss.str()));
    return Executor::passthrough(cmd).ok();
}

// ---------- 用户路径 ----------

std::string SystemHelper::user_home() {
#ifndef _WIN32
    const char* sudo_user = std::getenv("SUDO_USER");
    if (sudo_user && sudo_user[0]) {
        // 废弃原版存在严重注入漏洞的 getent shell 调用，改用内存级 POSIX API
        struct passwd* pw = getpwnam(sudo_user);
        if (pw && pw->pw_dir) {
            return std::string(pw->pw_dir);
        }
    }
#endif
    const char* home = std::getenv("HOME");
    return home ? home : "/root";
}

// ---------- 下载 / 解压 ----------

bool SystemHelper::download_file(const std::string &url, const std::string &dest_path) {
    auto curl_cmd = CommandBuilder("curl").add_flag("-#fL").add_arg(url).add_arg("-o").add_arg(dest_path).build_string();
    auto wget_cmd = CommandBuilder("wget").add_flag("--show-progress").add_arg("-O").add_arg(dest_path).add_arg(url).build_string();
    auto aria2_cmd = CommandBuilder("aria2c").add_flag("--no-conf").add_flag("--allow-overwrite=true").add_arg("-o").add_arg(dest_path).add_arg(url).build_string();

    // 控制流由 Shell 剥离回 C++，避免管道挂死
    if (Executor::has("curl") && Executor::passthrough(curl_cmd).ok()) return true;
    if (Executor::has("wget") && Executor::passthrough(wget_cmd).ok()) return true;
    if (Executor::has("aria2c") && Executor::passthrough(aria2_cmd).ok()) return true;

    return fs::exists(dest_path);
}

std::string SystemHelper::http_get_cached(const std::string &url, const std::string &cache_name) {
    // 使用带进程号和随机数的临时路径，防止缓存被 Symlink 劫持
    fs::path cache_path = generate_temp_path(cache_name);
    download_file(url, cache_path.string());
    std::string data = read_file(cache_path);
    std::error_code ec;
    fs::remove(cache_path, ec);
    return data;
}

void SystemHelper::extract_archive(const std::string &archive_path, const std::string &dest) {
    bool has_pv = Executor::has("pv");
    std::string safe_archive = safe_escape(archive_path);
    std::string safe_dest = safe_escape(dest);

    // --- 1. 创建隔离环境 ---
    // SANDBOX 用于存放解压中间物，PAYLOAD 用于存放提取出的纯净系统树
    std::string prep_cmd = "SANDBOX=$(mktemp -d); PAYLOAD=$SANDBOX/payload; mkdir -p $PAYLOAD";

    // --- 2. 各格式解压到 PAYLOAD 的命令 ---

    // tar: 动态探测并直接解压数据到 PAYLOAD
    std::string tar_cmd =
        "FTYPE=$(file -b " + safe_archive + " | tr '[:upper:]' '[:lower:]'); "
        "if echo \"$FTYPE\" | grep -q 'zstandard'; then DEC='zstd -d -c'; "
        "elif echo \"$FTYPE\" | grep -q 'xz'; then DEC='xz -d -c'; "
        "elif echo \"$FTYPE\" | grep -q 'gzip'; then DEC='gzip -d -c'; "
        "elif echo \"$FTYPE\" | grep -q 'bzip2'; then DEC='bzip2 -d -c'; "
        "else DEC='cat'; fi; "
        + (has_pv ? "pv " + safe_archive + " | $DEC 2>/dev/null | sudo tar -xf - -C $PAYLOAD"
                  : "$DEC " + safe_archive + " 2>/dev/null | sudo tar -xf - -C $PAYLOAD");

    // deb: 在 SANDBOX 解开 ar 外壳，【只把其中的 data.tar.*】 解压到 PAYLOAD
    std::string deb_cmd =
        "cd $SANDBOX && sudo ar x " + safe_archive + " 2>/dev/null && "
        "for f in data.tar.*; do [ -e \"$f\" ] && sudo tar -xaf \"$f\" -C $PAYLOAD 2>/dev/null; done";

    // zip: 解压到 PAYLOAD
    std::string zip_cmd = "sudo unzip -q -o " + safe_archive + " -d $PAYLOAD";

    // zst兜底: 单文件时生成特定文件
    std::string zst_cmd = "sudo sh -c 'zstd -d -c " + safe_archive + " > $PAYLOAD/extracted_file'";

    // --- 3. 尊重作者意图的“目录树合并”逻辑 ---
    std::string smart_transfer =
        // 修复部分 Github 源码包恶心的单层外壳嵌套 (例如 解压后变成 /payload/ubuntu-wallpapers-master/usr/...)
        "if [ $(ls -1qA $PAYLOAD | wc -l) -eq 1 ]; then "
        "  WRAPPER=$(ls -A $PAYLOAD); "
        "  if [ -d \"$PAYLOAD/$WRAPPER/usr\" ]; then "
        "    sudo mv \"$PAYLOAD/$WRAPPER/\"* \"$PAYLOAD/\" 2>/dev/null; "
        "  fi; "
        "fi; "

        // 核心合并逻辑：判定这是否是一个具有系统目录结构的包
        "if [ -d \"$PAYLOAD/usr\" ]; then "
        // 这是一个标准包：作者意图是按系统层级覆盖。
        // 我们提取 usr, etc, opt, var, lib 等标准目录并直接合并到系统根目录 '/'
        // 这样不仅保留了嵌套结构，还能完美过滤掉 Arch 留在包根目录下的 '.PKGINFO' 等垃圾文件！
        "  for sysdir in usr etc opt var lib; do "
        "    if [ -d \"$PAYLOAD/$sysdir\" ]; then "
        "      sudo cp -rf \"$PAYLOAD/$sysdir\" / 2>/dev/null; "
        "    fi; "
        "  done; "
        "else "
        // 这是一个平铺的文件包（例如只有 .jpg 文件），没有系统层级
        // 此时我们才动用 dest 参数作为兜底位置
        "  sudo mkdir -p " + safe_dest + "; "
        "  sudo cp -rf \"$PAYLOAD/\"* " + safe_dest + "/ 2>/dev/null; "
        "fi";

    // --- 4. 组合执行流程 ---
    std::string extract_cmd =
        prep_cmd + " && "
        "( "
        "  (echo '  => tar...' && (" + tar_cmd + ") && echo '  [OK] tar') || "
        "  (echo '  => deb...' && (" + deb_cmd + ") && echo '  [OK] deb') || "
        "  (echo '  => zip...' && (" + zip_cmd + ") && echo '  [OK] zip') || "
        "  (echo '  => zst...' && (" + zst_cmd + ") && echo '  [OK] zst') || "
        "  (echo '  [FAIL]' && false) "
        ") && "
        "(" + smart_transfer + ") ; "
        "STATUS=$?; "
        "sudo rm -rf $SANDBOX; "
        "exit $STATUS";

    auto result = Executor::passthrough(extract_cmd);
    if (result.ok())
        Logger::ok(_f("gui.extract_ok", dest));
    else
        Logger::error(_f("gui.extract_fail", dest));
}

std::string SystemHelper::find_latest_href(const std::string &html, const std::string &pkg_pattern) {
    std::regex href_re(R"re(<a\s+[^>]*href\s*=\s*"([^"]*)"[^>]*>)re", std::regex::icase);
    std::regex pkg_re(pkg_pattern);
    std::string best;
    auto begin = std::sregex_iterator(html.begin(), html.end(), href_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string href = (*it)[1].str();
        if (std::regex_search(href, pkg_re)) {
            best = href;
        }
    }
    return best;
}

bool SystemHelper::fetch_latest_and_extract(const std::string &repo_url,
                                             const std::string &pkg_pattern,
                                             const std::string &tmp_prefix,
                                             const std::string &extract_to) {
    std::string html = http_get_cached(repo_url, tmp_prefix + "_index");
    if (html.empty()) return false;

    std::string pkg_name = find_latest_href(html, pkg_pattern);
    if (pkg_name.empty()) return false;

    // 使用安全临时路径替换硬编码
    fs::path pkg_path = generate_temp_path(tmp_prefix + "_pkg");
    if (!download_file(repo_url + pkg_name, pkg_path.string())) return false;
    // 验证下载的是有效归档文件，不是 HTML 错误页
    {
        auto hdr = read_file(pkg_path.string());
        if (hdr.size() >= 2 && (hdr.substr(0, 2) == "<!" || hdr.substr(0, 2) == "<?")) {
            std::error_code ec;
            fs::remove(pkg_path, ec);
            return false;
        }
    }

    extract_archive(pkg_path.string(), extract_to);

    std::error_code ec;
    fs::remove(pkg_path, ec);
    return true;
}

bool SystemHelper::git_clone_and_extract(const std::string &git_url,
                                          const std::string &branch,
                                          const std::string &archive_name,
                                          const std::string &tmp_dir,
                                          const std::string &extract_to) {
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
    fs::create_directories(tmp_dir, ec);

    auto clone_cmd = CommandBuilder("git")
        .add_arg("clone").add_arg("-b").add_arg(branch)
        .add_flag("--depth=1").add_arg(git_url).add_arg("repo")
        .add_raw("2>/dev/null").build_string();
    auto tar_cmd = CommandBuilder("tar")
        .add_flag("-Jxf").add_arg("repo/" + archive_name)
        .add_arg("-C").add_arg(extract_to)
        .add_raw("2>/dev/null").build_string();

    Executor::passthrough("cd " + safe_escape(tmp_dir) + " && " + clone_cmd + " && " + tar_cmd);
    fs::remove_all(tmp_dir, ec);
    return true;
}

std::string SystemHelper::user_pictures_dir() {
    // XDG 标准：优先 $XDG_PICTURES_DIR，其次 ~/.config/user-dirs.dirs，兜底 ~/Pictures
    const char* xdg = std::getenv("XDG_PICTURES_DIR");
    if (xdg && xdg[0]) return xdg;

    std::string dirs_file = user_home() + "/.config/user-dirs.dirs";
    auto content = read_file(dirs_file);
    if (!content.empty()) {
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            // 格式: XDG_PICTURES_DIR="$HOME/Pictures"
            if (line.find("XDG_PICTURES_DIR=") != std::string::npos) {
                auto start = line.find('"');
                auto end = line.rfind('"');
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    std::string dir = line.substr(start + 1, end - start - 1);
                    // 展开 $HOME
                    auto home_pos = dir.find("$HOME");
                    if (home_pos != std::string::npos)
                        dir.replace(home_pos, 5, user_home());
                    return dir;
                }
            }
        }
    }
    return user_home() + "/Pictures";
}

} // namespace tmoe