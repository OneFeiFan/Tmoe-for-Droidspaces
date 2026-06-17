#include "domain/backup.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace tmoe::domain {

BackupManager::BackupManager(const TmoeConfig& cfg) : cfg_(cfg) {}

// ═══════════════════════════════════════════════════════════════
//  TUI 主菜单
// ═══════════════════════════════════════════════════════════════

void BackupManager::run_backup_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"📦 备份与恢复\" --menu \"请选择操作:\" 0 0 0 "
            "\"1\" \"💾 备份 Linux 容器 (tar.zst/xz)\" "
            "\"2\" \"📥 恢复 Linux 容器\" "
            "\"3\" \"🧹 清理容器垃圾\" "
            "\"4\" \"💿 备份至外置 SD/TF 卡\" "
            "\"5\" \"📋 列出所有备份文件\" "
            "\"0\" \"返回上级菜单\"";

        std::string choice = Executor::tui_select(menu);
        if (choice == "0" || choice.empty()) break;

        if (choice == "1") {
            // 备份容器：TUI 输入容器名和路径
            std::string name_cmd = cfg_.tui_bin +
                " --title \"容器名\" --inputbox \"请输入容器名称 (如 debian):\" 0 0 \"debian\"";
            auto result = Executor::shell("echo \"$(" + name_cmd + " 2>&1 1>/dev/tty)\"");
            std::string name = result.stdout_data;
            name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
            if (name.empty()) continue;

            std::string path_cmd = cfg_.tui_bin +
                " --title \"Rootfs 路径\" --inputbox \"请输入容器 rootfs 路径:\" 0 0 \"" +
                (cfg_.container_root / name).string() + "\"";
            result = Executor::shell("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
            std::string rootfs = result.stdout_data;
            rootfs.erase(std::remove(rootfs.begin(), rootfs.end(), '\n'), rootfs.end());
            if (rootfs.empty()) continue;

            if (fs::exists(rootfs)) {
                backup_container(name, rootfs);
            } else {
                Logger::error("路径不存在: " + rootfs);
            }
        } else if (choice == "2") {
            restore_container();
        } else if (choice == "3") {
            // 输入容器路径并清理垃圾
            std::string path_cmd = cfg_.tui_bin +
                " --title \"容器路径\" --inputbox \"请输入容器 rootfs 路径:\" 0 0 \"" +
                cfg_.container_root.string() + "\"";
            auto result = Executor::shell("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
            std::string rootfs = result.stdout_data;
            rootfs.erase(std::remove(rootfs.begin(), rootfs.end(), '\n'), rootfs.end());
            if (!rootfs.empty() && fs::exists(rootfs)) {
                clean_container_garbage(rootfs);
            }
        } else if (choice == "4") {
            std::string name_cmd = cfg_.tui_bin +
                " --title \"容器名\" --inputbox \"请输入容器名称:\" 0 0 \"debian\"";
            auto result = Executor::shell("echo \"$(" + name_cmd + " 2>&1 1>/dev/tty)\"");
            std::string name = result.stdout_data;
            name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
            if (!name.empty()) {
                backup_to_external_storage(name, (cfg_.container_root / name).string());
            }
        } else if (choice == "5") {
            auto backups = list_backups();
            if (backups.empty()) {
                Logger::info("未找到备份文件");
            } else {
                Logger::info("备份文件列表 (" + cfg_.backup_dir.string() + "):");
                for (const auto& f : backups) {
                    auto size_mb = get_archive_size_mb(cfg_.backup_dir.string() + "/" + f);
                    Logger::info("  📄 " + f + "  [" + std::to_string(size_mb) + " MB]");
                }
            }
        }
        Logger::press_enter();
    }
}

// ═══════════════════════════════════════════════════════════════
//  容器备份
// ═══════════════════════════════════════════════════════════════

bool BackupManager::backup_container(std::string_view container_name,
                                      std::string_view rootfs_path) {
    Logger::step("备份容器: " + std::string(container_name));

    // 检查 rootfs 路径是否存在
    if (!fs::exists(rootfs_path)) {
        Logger::error("容器 rootfs 路径不存在: " + std::string(rootfs_path));
        return false;
    }

    // 提示是否先清理垃圾
    bool do_cleanup = Logger::confirm("是否先清理容器垃圾再做备份 (推荐)?");
    if (do_cleanup) {
        clean_container_garbage(rootfs_path);
    }

    // 选择压缩格式
    ArchiveFormat fmt = tui_select_format();

    // 生成备份文件名
    std::string filename = generate_filename(std::string(container_name), "");
    if (fmt == ArchiveFormat::TarZst) filename += ".tar.zst";
    else if (fmt == ArchiveFormat::TarXz) filename += ".tar.xz";
    else filename += ".tar.gz";

    fs::path output_path = cfg_.backup_dir / filename;

    Logger::info("备份输出: " + output_path.string());
    Logger::info("正在打包压缩，请稍候...");

    std::vector<std::string> excludes;
    // 排除外部挂载的 SD 卡
    std::string sd_path = std::string(rootfs_path) + "/media/sd";
    if (fs::exists(sd_path)) {
        excludes.push_back(sd_path);
    }

    bool ok = run_tar_backup(std::string(rootfs_path), output_path.string(), fmt, excludes);
    if (ok) {
        auto size_mb = get_archive_size_mb(output_path.string());
        Logger::ok("备份完成: " + output_path.filename().string() +
                   " (" + std::to_string(size_mb) + " MB)");
        Logger::info("备份位置: " + output_path.string());
    } else {
        Logger::error("备份失败！");
    }
    return ok;
}

bool BackupManager::backup_to_external_storage(std::string_view container_name,
                                                 std::string_view rootfs_path) {
    Logger::step("备份至外置 SD/TF 卡: " + std::string(container_name));

    // 检查外部存储
    std::vector<std::string> candidates = {
        "/sdcard", "/storage/emulated/0",
        "/media/sd", "/mnt/sdcard"
    };

    std::string ext_storage;
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            ext_storage = p;
            break;
        }
    }

    if (ext_storage.empty()) {
        Logger::error("未找到外部存储设备 (SD/TF 卡)");
        return false;
    }

    fs::path ext_backup = fs::path(ext_storage) / "tmoe-backup";
    if (!fs::exists(ext_backup)) {
        fs::create_directories(ext_backup);
    }

    ArchiveFormat fmt = tui_select_format();
    bool do_cleanup = Logger::confirm("是否先清理容器垃圾?");
    if (do_cleanup) clean_container_garbage(rootfs_path);

    std::string filename = generate_filename(std::string(container_name), "");
    if (fmt == ArchiveFormat::TarZst) filename += ".tar.zst";
    else if (fmt == ArchiveFormat::TarXz) filename += ".tar.xz";
    else filename += ".tar.gz";

    fs::path output_path = ext_backup / filename;
    Logger::info("备份输出: " + output_path.string());

    std::vector<std::string> excludes;
    bool ok = run_tar_backup(std::string(rootfs_path), output_path.string(), fmt, excludes);
    if (ok) {
        auto size_mb = get_archive_size_mb(output_path.string());
        Logger::ok("备份至外置存储完成: " + output_path.string() +
                   " (" + std::to_string(size_mb) + " MB)");
    }
    return ok;
}

bool BackupManager::clean_container_garbage(std::string_view rootfs_path) {
    Logger::step("清理容器垃圾文件...");

    std::string root(rootfs_path);
    std::ostringstream cmd;

    // 清理 apt 缓存
    cmd << "rm -rf " << root << "/var/cache/apt/archives/*.deb"
        << " " << root << "/var/lib/apt/lists/*"
        << " " << root << "/var/cache/man/*";

    // 清理历史记录
    cmd << " " << root << "/root/.bash_history"
        << " " << root << "/home/*/.bash_history";

    // 清理日志
    cmd << " " << root << "/var/log/*.log"
        << " " << root << "/var/log/*.gz"
        << " " << root << "/var/log/apt/*";

    // 清理临时文件
    cmd << " " << root << "/tmp/*"
        << " " << root << "/var/tmp/*";

    // 清理 pip/npm 缓存 (如存在)
    cmd << " " << root << "/root/.cache/pip"
        << " " << root << "/root/.npm/_cacache";

    cmd << " 2>/dev/null";

    auto result = Executor::shell(cmd.str());
    Logger::ok("容器垃圾已清理");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  容器恢复
// ═══════════════════════════════════════════════════════════════

bool BackupManager::restore_container(std::string_view archive_path) {
    Logger::step("恢复容器...");

    RestoreMode mode = tui_select_restore_mode();
    std::string archive;

    switch (mode) {
        case RestoreMode::Normal: {
            // 从默认备份目录恢复
            auto latest = detect_latest_backup();
            if (!latest.empty()) {
                Logger::info("检测到最新备份: " + latest);
                if (Logger::confirm("使用此备份还原?")) {
                    archive = cfg_.backup_dir.string() + "/" + latest;
                } else {
                    archive = tui_select_file(cfg_.backup_dir.string());
                }
            } else {
                archive = tui_select_file(cfg_.backup_dir.string());
            }
            break;
        }
        case RestoreMode::FromSD: {
            std::string sd_backup = "/sdcard/tmoe-backup";
            if (!fs::exists(sd_backup)) {
                Logger::error("外置存储备份目录不存在: " + sd_backup);
                return false;
            }
            archive = tui_select_file(sd_backup);
            break;
        }
        case RestoreMode::ManualPath: {
            std::string path_cmd = cfg_.tui_bin +
                " --title \"选择文件\" --inputbox \"请输入备份文件完整路径:\" 0 0";
            auto result = Executor::shell("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
            archive = result.stdout_data;
            archive.erase(std::remove(archive.begin(), archive.end(), '\n'), archive.end());
            break;
        }
        case RestoreMode::FromDownload: {
            std::string dl_path = "/sdcard/Download/backup";
            if (!fs::exists(dl_path)) {
                Logger::error("下载备份目录不存在: " + dl_path);
                return false;
            }
            archive = tui_select_file(dl_path);
            break;
        }
        case RestoreMode::Compat: {
            // 兼容模式：直接解压到 /，不检查路径
            auto latest = detect_latest_backup();
            if (!latest.empty()) {
                archive = cfg_.backup_dir.string() + "/" + latest;
            } else {
                archive = tui_select_file(cfg_.backup_dir.string());
            }
            break;
        }
    }

    if (archive.empty()) {
        Logger::warn("未选择备份文件");
        return false;
    }

    if (!fs::exists(archive)) {
        Logger::error("备份文件不存在: " + archive);
        return false;
    }

    // 确认目标路径
    std::string target_cmd = cfg_.tui_bin +
        " --title \"还原目标\" --inputbox \"请输入还原目标路径:\" 0 0 \"" +
        cfg_.container_root.string() + "\"";
    auto result = Executor::shell("echo \"$(" + target_cmd + " 2>&1 1>/dev/tty)\"");
    std::string target = result.stdout_data;
    target.erase(std::remove(target.begin(), target.end(), '\n'), target.end());

    if (target.empty()) {
        Logger::error("未指定还原目标路径");
        return false;
    }

    // 还原前警告
    Logger::warn("⚠️ 还原将覆盖目标路径的内容！");
    if (!Logger::confirm("确认继续还原?")) {
        Logger::info("已取消");
        return false;
    }

    Logger::info("正在还原: " + archive + " → " + target);
    bool ok = uncompress_archive(archive, target);

    if (ok) {
        Logger::ok("还原完成！容器已恢复至: " + target);
    } else {
        Logger::error("还原失败！请检查磁盘空间和备份文件完整性。");
    }
    return ok;
}

bool BackupManager::uncompress_archive(std::string_view archive_path,
                                         std::string_view target_path) {
    ArchiveFormat fmt = detect_format(archive_path);
    std::string tar_cmd;

    switch (fmt) {
        case ArchiveFormat::TarZst:
            tar_cmd = "tar --use-compress-program zstd -Ppxvf \"" +
                      std::string(archive_path) + "\" -C \"" +
                      std::string(target_path) + "\"";
            break;
        case ArchiveFormat::TarXz:
            tar_cmd = "tar -PpJxvf \"" + std::string(archive_path) +
                      "\" -C \"" + std::string(target_path) + "\"";
            break;
        case ArchiveFormat::TarGz:
            tar_cmd = "tar -Ppzxvf \"" + std::string(archive_path) +
                      "\" -C \"" + std::string(target_path) + "\"";
            break;
        default:
            Logger::error("不支持的备份格式: " + std::string(archive_path));
            return false;
    }

    return Executor::shell(tar_cmd).ok();
}

// ═══════════════════════════════════════════════════════════════
//  辅助函数
// ═══════════════════════════════════════════════════════════════

bool BackupManager::run_tar_backup(std::string_view source_dir,
                                     std::string_view output_file,
                                     ArchiveFormat format,
                                     const std::vector<std::string>& excludes) {
    // 确保备份目录存在
    fs::path out(output_file);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::ostringstream cmd;

    switch (format) {
        case ArchiveFormat::TarZst:
            cmd << "tar --use-compress-program zstd -Ppcvf \""
                << output_file << "\"";
            break;
        case ArchiveFormat::TarXz:
            cmd << "tar -PJpcvf \"" << output_file << "\"";
            break;
        case ArchiveFormat::TarGz:
            cmd << "tar -Ppzcvf \"" << output_file << "\"";
            break;
        default:
            Logger::error("不支持的备份格式");
            return false;
    }

    // 排除路径
    for (const auto& ex : excludes) {
        cmd << " --exclude=\"" << ex << "\"";
    }

    cmd << " \"" << source_dir << "\"";

    Logger::debug("执行: " + cmd.str());
    return Executor::shell(cmd.str()).ok();
}

ArchiveFormat BackupManager::tui_select_format() {
    std::string menu = cfg_.tui_bin +
        " --title \"压缩格式\" --menu \"请选择备份压缩格式:\" 0 0 0 "
        "\"1\" \"zstd (推荐 — 速度快)\" "
        "\"2\" \"xz (体积最小)\" "
        "\"3\" \"gzip (兼容性最好)\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "2") return ArchiveFormat::TarXz;
    if (choice == "3") return ArchiveFormat::TarGz;
    return ArchiveFormat::TarZst;  // 默认 zstd
}

BackupManager::RestoreMode BackupManager::tui_select_restore_mode() {
    std::string menu = cfg_.tui_bin +
        " --title \"恢复模式\" --menu \"请选择恢复方式:\" 0 0 0 "
        "\"1\" \"📦 常规模式 (从默认备份路径恢复)\" "
        "\"2\" \"💿 从外置 SD/TF 卡恢复\" "
        "\"3\" \"📂 手动选择路径\" "
        "\"4\" \"📥 从 Download/backup 恢复\" "
        "\"5\" \"🔧 兼容模式 (直接解压到根目录)\"";

    std::string choice = Executor::tui_select(menu);
    if (choice == "2") return RestoreMode::FromSD;
    if (choice == "3") return RestoreMode::ManualPath;
    if (choice == "4") return RestoreMode::FromDownload;
    if (choice == "5") return RestoreMode::Compat;
    return RestoreMode::Normal;
}

std::string BackupManager::tui_select_file(const std::string& base_dir) {
    // 使用 whiptail --fselect 选择文件
    std::string cmd = cfg_.tui_bin +
        " --title \"选择备份文件\" --fselect \"" + base_dir + "/\" 0 0";
    auto result = Executor::shell(cmd + " 2>&1");
    // fselect 输出到 stderr
    std::string selected = result.stderr_data.empty() ?
                           result.stdout_data : result.stderr_data;
    selected.erase(std::remove(selected.begin(), selected.end(), '\n'), selected.end());
    selected.erase(std::remove(selected.begin(), selected.end(), '\r'), selected.end());
    return selected;
}

std::vector<std::string> BackupManager::list_backups() const {
    std::vector<std::string> result;
    std::string backup_dir = cfg_.backup_dir.string();

    if (!fs::exists(backup_dir)) return result;

    for (const auto& entry : fs::directory_iterator(backup_dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        auto name = entry.path().filename().string();
        // 匹配 .zst / .xz / .gz 结尾
        if (name.find(".tar.zst") != std::string::npos ||
            name.find(".tar.xz") != std::string::npos ||
            name.find(".tar.gz") != std::string::npos) {
            result.push_back(name);
        }
    }

    // 按修改时间降序排列（最新的在前）
    std::sort(result.begin(), result.end(),
              [&](const std::string& a, const std::string& b) {
                  return fs::last_write_time(fs::path(backup_dir) / a) >
                         fs::last_write_time(fs::path(backup_dir) / b);
              });

    return result;
}

std::string BackupManager::detect_latest_backup() const {
    auto backups = list_backups();
    if (backups.empty()) return "";
    return backups.front();  // 已按时间降序排列
}

ArchiveFormat BackupManager::detect_format(std::string_view path) {
    std::string p(path);
    if (p.find(".tar.zst") != std::string::npos) return ArchiveFormat::TarZst;
    if (p.find(".tar.xz") != std::string::npos) return ArchiveFormat::TarXz;
    if (p.find(".tar.gz") != std::string::npos) return ArchiveFormat::TarGz;
    if (p.find(".tgz") != std::string::npos) return ArchiveFormat::TarGz;
    return ArchiveFormat::Unknown;
}

std::string BackupManager::generate_filename(std::string_view container_name,
                                               std::string_view distro,
                                               std::string_view desktop) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S") << "_";

    if (!desktop.empty()) {
        oss << container_name << "+" << desktop;
    } else {
        oss << container_name;
    }

    return oss.str();
}

int64_t BackupManager::get_archive_size_mb(std::string_view path) {
    try {
        return fs::file_size(path) / (1024 * 1024);
    } catch (...) {
        return 0;
    }
}

} // namespace tmoe::domain
