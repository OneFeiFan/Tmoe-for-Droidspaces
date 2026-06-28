#include "system_helper.h"
#include "core/logger.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"
#include <fstream>
#include <sstream>

namespace tmoe {

// ---------- 文件 I/O ----------

namespace {
    // 直接问内核：当前进程能否写此路径？（文件不存在时检查父目录）
    bool can_write_to(const fs::path &path) {
#ifdef _WIN32
        return true;
#else
        std::error_code ec;
        if (fs::exists(path, ec)) {
            return access(path.c_str(), W_OK) == 0;
        }
        return access(path.parent_path().c_str(), W_OK) == 0;
#endif
    }

    bool write_via_shell(const fs::path &path, std::string_view content, bool append) {
        std::string escaped(content);
        size_t pos = 0;
        while ((pos = escaped.find('\'', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "'\\''");
            pos += 4;
        }
        std::string op = append ? "tee -a" : "tee";
        auto r = Executor::shell("echo '" + escaped + "' | sudo " + op + " '" +
                                 path.string() + "' > /dev/null 2>&1");
        return r.ok();
    }

    bool do_write(const fs::path &path, std::string_view content, bool append) {
        fs::create_directories(path.parent_path());
        auto mode = append ? (std::ios::app | std::ios::out) : std::ios::trunc;
        std::ofstream ofs(path, mode);
        if (!ofs.is_open()) return false;
        ofs << content;
        return ofs.good();
    }
}

bool SystemHelper::write_file(const fs::path &path, std::string_view content) {
    if (!can_write_to(path)) {
        return write_via_shell(path, content, false);
    }
    try {
        return do_write(path, content, false);
    } catch (const std::exception &e) {
        Logger::error(std::string("Write file failed: ") + path.string() + " — " + e.what());
        return false;
    }
}

bool SystemHelper::append_file(const fs::path &path, std::string_view content) {
    if (!can_write_to(path)) {
        return write_via_shell(path, content, true);
    }
    try {
        return do_write(path, content, true);
    } catch (const std::exception &e) {
        Logger::error(std::string("Append file failed: ") + path.string() + " — " + e.what());
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

    // ---------- 下载 / 解压 ----------

bool SystemHelper::download_file(const std::string &url, const std::string &dest_path) {
    // curl -# = 显示进度条；wget --show-progress；aria2c 无 -q 显示控制台进度
    auto curl_cmd = CommandBuilder("curl")
        .add_flag("-#L").add_arg(url)
        .add_arg("-o").add_arg(dest_path)
        .build_string();
    auto wget_cmd = CommandBuilder("wget")
        .add_flag("--show-progress").add_arg("-O").add_arg(dest_path).add_arg(url)
        .build_string();
    auto aria2_cmd = CommandBuilder("aria2c")
        .add_flag("--no-conf").add_flag("--allow-overwrite=true")
        .add_arg("-o").add_arg(dest_path).add_arg(url)
        .build_string();

    Executor::passthrough("(" + curl_cmd + " || " + wget_cmd + " || " + aria2_cmd + ")");
    return fs::exists(dest_path);
}

std::string SystemHelper::http_get_cached(const std::string &url, const std::string &cache_name) {
    fs::path cache_path = fs::path("/tmp") / cache_name;
    download_file(url, cache_path.string());
    return read_file(cache_path);
}

void SystemHelper::extract_archive(const std::string &archive_path, const std::string &dest) {
    bool has_pv = Executor::has("pv");

    // tar: pv 管道显示进度，回退到直接 tar
    std::string tar_cmd;
    if (has_pv) {
        tar_cmd = std::string("pv \"") + archive_path + "\" | tar -xf -";
    } else {
        tar_cmd = std::string("tar -xf \"") + archive_path + "\"";
    }

    // deb: ar 解出 data.tar.xz → pv 管道 → tar
    std::string deb_cmd;
    if (has_pv) {
        deb_cmd = std::string("ar x \"") + archive_path +
                  "\" data.tar.xz 2>/dev/null && "
                  "pv data.tar.xz | tar -Jxf - && rm -f data.tar.xz";
    } else {
        deb_cmd = std::string("ar x \"") + archive_path +
                  "\" data.tar.xz 2>/dev/null && "
                  "tar -Jxf data.tar.xz && rm -f data.tar.xz";
    }

    // unzip: 自带进度条（无 -q）
    auto zip_cmd = CommandBuilder("unzip")
        .add_flag("-o").add_arg(archive_path).add_arg("-d").add_arg(dest)
        .build_string();

    Executor::passthrough(
        "cd " + dest + " && (" + tar_cmd + " || (" + deb_cmd + ") || " + zip_cmd + " || true)");
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
    std::string html = http_get_cached(repo_url, "." + tmp_prefix + "_index.html");
    if (html.empty()) return false;

    std::string pkg_name = find_latest_href(html, pkg_pattern);
    if (pkg_name.empty()) return false;

    std::string pkg_path = "/tmp/." + tmp_prefix + "_pkg";
    if (!download_file(repo_url + pkg_name, pkg_path)) return false;

    extract_archive(pkg_path, extract_to);

    std::error_code ec;
    fs::remove(pkg_path, ec);
    fs::remove("/tmp/." + tmp_prefix + "_index.html", ec);
    fs::remove("/tmp/data.tar.xz", ec);
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

    Executor::passthrough("cd " + tmp_dir + " && " + clone_cmd + " && " + tar_cmd);
    fs::remove_all(tmp_dir, ec);
    return true;
}

} // namespace tmoe
