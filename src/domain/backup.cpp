#include "domain/backup.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace tmoe::domain {

BackupManager::BackupManager(const TmoeConfig& cfg) : cfg_(cfg) {}

// ═══════════════════════════════════════════════════════════════
//  TUI 主菜单
// ═══════════════════════════════════════════════════════════════

void BackupManager::run_backup_menu() {
    while (true) {
        std::string menu = cfg_.tui_bin +
            " --title \"📦 备份与恢复\" --menu \"请选择操作:\" 0 0 0 "
            "\"1\" \"💾 备份 Linux 容器 (tar.zst/xz/gz)\" "
            "\"2\" \"📥 恢复 Linux 容器\" "
            "\"3\" \"🧹 清理容器垃圾 (多发行版)\" "
            "\"4\" \"💿 备份至外置 SD/TF 卡\" "
            "\"5\" \"📋 列出所有备份文件\" "
            "\"6\" \"📊 容器空间占用排行 (Top 30)\" "
            "\"7\" \"🕐 Timeshift 系统快照\" "
            "\"8\" \"📱 Termux 宿主备份\" "
            "\"9\" \"📂 查看备份目录大小\" "
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
                show_garbage_stats(rootfs);
                if (Logger::confirm("是否执行清理?")) {
                    clean_container_garbage(rootfs);
                    Logger::info("\n清理后磁盘空间:");
                    show_garbage_stats(rootfs);
                }
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
            auto backups = list_backups_detailed();
            if (backups.empty()) {
                Logger::info("未找到备份文件");
            } else {
                Logger::info("备份文件列表 (" + cfg_.backup_dir.string() + "):");
                for (const auto& [name, size_mb, mtime] : backups) {
                    Logger::info("  📄 " + name + "  [" + std::to_string(size_mb) + " MB]  " + mtime);
                }
            }
        } else if (choice == "6") {
            // 空间占用排行
            std::string path_cmd = cfg_.tui_bin +
                " --title \"容器路径\" --inputbox \"请输入容器 rootfs 路径:\" 0 0 \"" +
                cfg_.container_root.string() + "\"";
            auto result = Executor::shell("echo \"$(" + path_cmd + " 2>&1 1>/dev/tty)\"");
            std::string rootfs = result.stdout_data;
            rootfs.erase(std::remove(rootfs.begin(), rootfs.end(), '\n'), rootfs.end());
            if (!rootfs.empty() && fs::exists(rootfs)) {
                show_garbage_stats(rootfs);
            }
        } else if (choice == "7") {
            // Timeshift
            std::string ts_menu = cfg_.tui_bin +
                " --title \"Timeshift 系统快照\" --menu \"Timeshift 操作:\" 0 0 0 "
                "\"1\" \"📸 创建快照\" "
                "\"2\" \"📥 安装 Timeshift\" "
                "\"0\" \"返回\"";
            std::string ts_choice = Executor::tui_select(ts_menu);
            if (ts_choice == "1") run_timeshift_backup();
            else if (ts_choice == "2") install_timeshift();
        } else if (choice == "8") {
            // Termux 宿主备份
            if (!cfg_.is_termux) {
                Logger::warn("此功能仅适用于 Termux 环境");
            } else {
                std::string tb_cmd = cfg_.tui_bin +
                    " --title \"Termux 宿主备份\" --checklist \"选择备份内容:\" 0 0 0 "
                    "\"home\" \"Termux Home 目录 (用户文件)\" ON "
                    "\"usr\" \"Termux Prefix (/data/data/com.termux/files/usr)\" OFF";
                auto result = Executor::shell(tb_cmd + " 2>&1");
                bool backup_home = result.stderr_data.find("\"home\"") != std::string::npos;
                bool backup_usr = result.stderr_data.find("\"usr\"") != std::string::npos;

                if (!backup_home && !backup_usr) {
                    Logger::warn("未选择任何备份内容");
                    continue;
                }

                std::string termux_prefix = "/data/data/com.termux/files";
                ArchiveFormat fmt = tui_select_format();
                std::string ext = (fmt == ArchiveFormat::TarZst) ? ".tar.zst" :
                                  (fmt == ArchiveFormat::TarXz) ? ".tar.xz" : ".tar.gz";

                fs::path out_dir = fs::path("/sdcard/Download/backup/termux");
                if (!fs::exists(out_dir)) fs::create_directories(out_dir);

                if (backup_home) {
                    std::string home_file = "termux-home-" + generate_filename("home", "", "") + ext;
                    Logger::step("备份 Termux Home...");
                    run_tar_backup_with_progress(termux_prefix + "/home",
                                                  (out_dir / home_file).string(), fmt);
                }
                if (backup_usr) {
                    std::vector<std::string> excludes = {termux_prefix + "/home/tmoe-linux/containers"};
                    std::string usr_file = "termux-usr-" + generate_filename("usr", "", "") + ext;
                    Logger::step("备份 Termux Prefix...");
                    run_tar_backup_with_progress(termux_prefix + "/usr",
                                                  (out_dir / usr_file).string(), fmt, excludes);
                }
                Logger::ok("Termux 备份完成: " + out_dir.string());
            }
        } else if (choice == "9") {
            // 备份目录大小
            std::string backup_dir = cfg_.backup_dir.string();
            if (!fs::exists(backup_dir)) {
                Logger::info("备份目录不存在: " + backup_dir);
            } else {
                auto result = Executor::shell("du -sh \"" + backup_dir + "\" 2>/dev/null");
                Logger::info("备份目录大小: " + backup_dir);
                Logger::info("  " + result.stdout_data);
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

    // 检测是否为 chroot 容器
    bool is_chroot = is_chroot_container(rootfs_path);
    if (is_chroot) {
        Logger::info("检测到 chroot 模式容器");
    }

    // 检测桌面环境
    std::string desktop = detect_desktop_environment(rootfs_path);
    if (!desktop.empty()) {
        Logger::info("检测到桌面环境: " + desktop);
    }

    // 提示是否先清理垃圾
    bool do_cleanup = Logger::confirm("是否先清理容器垃圾再做备份 (推荐)?");
    if (do_cleanup) {
        show_garbage_stats(rootfs_path);
        if (Logger::confirm("确认清理以上内容?")) {
            clean_container_garbage(rootfs_path);
        }
    }

    // 选择压缩格式
    ArchiveFormat fmt = tui_select_format();

    // 生成备份文件名（含 DE 名称和时间戳）
    std::string filename = generate_filename(std::string(container_name), "", desktop);
    if (fmt == ArchiveFormat::TarZst) filename += ".tar.zst";
    else if (fmt == ArchiveFormat::TarXz) filename += ".tar.xz";
    else filename += ".tar.gz";

    if (is_chroot) {
        // chroot 备份使用不同后缀
        size_t dot_pos = filename.rfind('.');
        if (dot_pos != std::string::npos) {
            filename = filename.substr(0, dot_pos);
        }
        std::string ext = (fmt == ArchiveFormat::TarZst) ? "tar.zst" :
                          (fmt == ArchiveFormat::TarXz) ? "tar.xz" : "tar.gz";
        filename = filename + "_chroot-rootfs_bak." + ext;
    }

    fs::path output_path = cfg_.backup_dir / filename;

    Logger::info("备份输出: " + output_path.string());

    // 排除路径
    std::vector<std::string> excludes;
    std::string root(rootfs_path);

    // 排除外部挂载
    for (const auto& mp : {"/media/sd", "/media/tf", "/media/termux", "/proc", "/sys", "/dev"}) {
        std::string full = root + mp;
        if (fs::exists(full)) excludes.push_back(full);
    }
    // 排除临时目录
    excludes.push_back(root + "/tmp");
    excludes.push_back(root + "/var/tmp");

    // 收集额外备份目标（tmoe 辅助文件）
    auto extra_targets = collect_extra_backup_targets(container_name);

    // 使用 pv 显示进度
    bool use_progress = has_pv() && !is_chroot;
    if (use_progress) {
        Logger::info("正在打包压缩（显示进度）...");
    } else {
        Logger::info("正在打包压缩，请稍候...");
    }

    bool ok;
    if (use_progress) {
        ok = run_tar_backup_with_progress(root, output_path.string(), fmt, excludes);
    } else {
        ok = run_tar_backup(root, output_path.string(), fmt, excludes);
    }

    // 备份附加目标（追加到 tar）
    if (ok && !extra_targets.empty()) {
        Logger::step("附加备份目标: tmoe 辅助文件/QEMU 库...");
        for (const auto& target : extra_targets) {
            if (fs::exists(target)) {
                std::string append_cmd = get_tar_prefix() +
                    "tar --use-compress-program zstd -Prvf \"" +
                    output_path.string() + "\" \"" + target + "\" 2>/dev/null";
                Executor::shell(append_cmd);
            }
        }
    }

    if (ok) {
        auto size_human = get_archive_size_human(output_path.string());
        Logger::ok("备份完成: " + output_path.filename().string() + " (" + size_human + ")");
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
        "/media/sd", "/mnt/sdcard",
        "/storage/0000-0000", "/storage/0000-0001"
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
    std::string desktop = detect_desktop_environment(rootfs_path);

    bool do_cleanup = Logger::confirm("是否先清理容器垃圾?");
    if (do_cleanup) clean_container_garbage(rootfs_path);

    std::string filename = generate_filename(std::string(container_name), "", desktop);
    if (fmt == ArchiveFormat::TarZst) filename += ".tar.zst";
    else if (fmt == ArchiveFormat::TarXz) filename += ".tar.xz";
    else filename += ".tar.gz";

    fs::path output_path = ext_backup / filename;
    Logger::info("备份输出: " + output_path.string());

    std::vector<std::string> excludes;
    std::string root(rootfs_path);
    for (const auto& mp : {"/media/sd", "/media/tf", "/media/termux", "/proc", "/sys", "/dev"}) {
        std::string full = root + mp;
        if (fs::exists(full)) excludes.push_back(full);
    }
    excludes.push_back(root + "/tmp");
    excludes.push_back(root + "/var/tmp");

    bool use_progress = has_pv() && !is_chroot_container(rootfs_path);
    bool ok;
    if (use_progress) {
        ok = run_tar_backup_with_progress(root, output_path.string(), fmt, excludes);
    } else {
        ok = run_tar_backup(root, output_path.string(), fmt, excludes);
    }

    if (ok) {
        auto size_human = get_archive_size_human(output_path.string());
        Logger::ok("备份至外置存储完成: " + output_path.string() + " (" + size_human + ")");
    }
    return ok;
}

// ═══════════════════════════════════════════════════════════════
//  容器清理（增强版：多发行版 + systemd + VNC + ZSH + 应用缓存）
// ═══════════════════════════════════════════════════════════════

bool BackupManager::clean_container_garbage(std::string_view rootfs_path) {
    Logger::step("清理容器垃圾文件...");

    std::string root(rootfs_path);

    // ── 1. 发行版包缓存 ──
    clean_package_cache(rootfs_path);

    // ── 2. systemd journal ──
    clean_systemd_journal(rootfs_path);

    // ── 3. 通用系统日志/缓存 ──
    std::ostringstream sys_cmd;
    sys_cmd << "rm -rf "
            << root << "/var/log/*.log "
            << root << "/var/log/*.gz "
            << root << "/var/log/*.1 "
            << root << "/var/log/*.old "
            << root << "/var/log/apt/* "
            << root << "/var/log/syslog* "
            << root << "/var/log/messages* "
            << root << "/var/log/kern* "
            << root << "/var/log/dpkg* "
            << root << "/var/mail/* "
            << root << "/var/crash/* "
            << " 2>/dev/null";
    Executor::shell(sys_cmd.str());

    // ── 4. 临时文件 ──
    std::ostringstream tmp_cmd;
    tmp_cmd << "rm -rf "
            << root << "/tmp/.* "
            << root << "/tmp/* "
            << root << "/var/tmp/* "
            << " 2>/dev/null";
    Executor::shell(tmp_cmd.str());

    // ── 5. 用户级垃圾（VNC/ZSH/应用缓存） ──
    clean_user_garbage(rootfs_path);

    Logger::ok("容器垃圾已清理");
    return true;
}

bool BackupManager::clean_package_cache(std::string_view rootfs_path) {
    std::string root(rootfs_path);
    std::string pkg_mgr = detect_package_manager(rootfs_path);

    if (pkg_mgr == "apt") {
        // Debian/Ubuntu
        Executor::shell("rm -rf " + root + "/var/cache/apt/archives/*.deb "
                        + root + "/var/cache/apt/archives/partial/* "
                        + root + "/var/cache/apt/*.bin "
                        + root + "/var/lib/apt/lists/* "
                        + root + "/var/lib/apt/lists/partial/* "
                        + root + "/var/cache/man/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 apt 缓存");
    } else if (pkg_mgr == "pacman") {
        // Arch/Manjaro
        Executor::shell("rm -rf " + root + "/var/cache/pacman/pkg/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 pacman 缓存");
    } else if (pkg_mgr == "dnf" || pkg_mgr == "yum") {
        // Fedora/RHEL
        Executor::shell("rm -rf " + root + "/var/cache/dnf/* "
                        + root + "/var/cache/yum/* "
                        + root + "/var/lib/dnf/yumdb/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 dnf/yum 缓存");
    } else if (pkg_mgr == "apk") {
        // Alpine
        Executor::shell("rm -rf " + root + "/var/cache/apk/* "
                        + root + "/etc/apk/cache/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 apk 缓存");
    } else if (pkg_mgr == "zypper") {
        // openSUSE
        Executor::shell("rm -rf " + root + "/var/cache/zypp/packages/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 zypper 缓存");
    } else if (pkg_mgr == "emerge") {
        // Gentoo
        Executor::shell("rm -rf " + root + "/usr/portage/distfiles/* "
                        + root + "/var/cache/distfiles/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 emerge 缓存");
    } else if (pkg_mgr == "xbps") {
        // Void
        Executor::shell("rm -rf " + root + "/var/cache/xbps/* "
                        + " 2>/dev/null");
        Logger::info("  已清理 xbps 缓存");
    }

    return true;
}

bool BackupManager::clean_systemd_journal(std::string_view rootfs_path) {
    std::string root(rootfs_path);
    std::string journal_dir = root + "/var/log/journal";

    if (fs::exists(journal_dir)) {
        Executor::shell("rm -rf \"" + journal_dir + "\"/* 2>/dev/null");
        Logger::info("  已清理 systemd journal 日志");
    }
    return true;
}

bool BackupManager::clean_user_garbage(std::string_view rootfs_path) {
    std::string root(rootfs_path);

    // 清理 root 和所有普通用户目录中的垃圾
    std::ostringstream cmd;

    // ── VNC 安全相关 ──
    cmd << "rm -rf "
        << root << "/root/.vnc/passwd "
        << root << "/root/.vnc/x11passwd "
        << root << "/home/*/.vnc/passwd "
        << root << "/home/*/.vnc/x11passwd ";

    // ── X 授权 ──
    cmd << root << "/root/.Xauthority "
        << root << "/root/.ICEauthority "
        << root << "/home/*/.Xauthority "
        << root << "/home/*/.ICEauthority ";

    // ── Shell 历史 ──
    cmd << root << "/root/.bash_history "
        << root << "/root/.zsh_history "
        << root << "/root/.zsh_compdump "
        << root << "/root/.zcompdump* "
        << root << "/root/.node_repl_history "
        << root << "/root/.python_history "
        << root << "/home/*/.bash_history "
        << root << "/home/*/.zsh_history "
        << root << "/home/*/.zsh_compdump "
        << root << "/home/*/.zcompdump* "
        << root << "/home/*/.node_repl_history "
        << root << "/home/*/.python_history ";

    // ── D-Bus / GNUPG / PKI ──
    cmd << root << "/root/.dbus "
        << root << "/root/.gnupg "
        << root << "/root/.pki "
        << root << "/home/*/.dbus "
        << root << "/home/*/.gnupg "
        << root << "/home/*/.pki ";

    // ── 浏览器缓存 ──
    cmd << root << "/root/.mozilla "
        << root << "/root/.cache/mozilla "
        << root << "/root/.config/chromium "
        << root << "/root/.cache/chromium "
        << root << "/home/*/.mozilla "
        << root << "/home/*/.cache/mozilla "
        << root << "/home/*/.config/chromium "
        << root << "/home/*/.cache/chromium ";

    // ── 应用缓存 ──
    cmd << root << "/root/.cache "
        << root << "/root/.config/Electron "
        << root << "/root/.config/pulse "
        << root << "/root/.cache/pip "
        << root << "/root/.local/share/pip "
        << root << "/root/.npm/_cacache "
        << root << "/home/*/.cache "
        << root << "/home/*/.config/Electron "
        << root << "/home/*/.config/pulse "
        << root << "/home/*/.cache/pip "
        << root << "/home/*/.local/share/pip "
        << root << "/home/*/.npm/_cacache ";

    // ── 语言特定缓存 ──
    cmd << root << "/root/.cache/pip3 "
        << root << "/home/*/.cache/pip3 ";
    // conda
    cmd << root << "/root/.conda/pkgs "
        << root << "/root/.conda/envs/*/.trash "
        << root << "/home/*/.conda/pkgs "
        << root << "/home/*/.conda/envs/*/.trash ";
    // cargo/go
    cmd << root << "/root/.cargo/registry/cache "
        << root << "/home/*/.cargo/registry/cache "
        << root << "/root/.cache/go-build "
        << root << "/home/*/.cache/go-build ";

    // ── 系统杂项 ──
    cmd << root << "/var/cache/man/* "
        << root << "/var/cache/ldconfig/aux-cache ";

    cmd << "2>/dev/null";

    Executor::shell(cmd.str());
    Logger::info("  已清理 VNC/X11 授权/Shell 历史/应用缓存");

    return true;
}

void BackupManager::show_garbage_stats(std::string_view rootfs_path) {
    std::string root(rootfs_path);

    // 整体大小
    auto result = Executor::shell("du -sh \"" + root + "\" 2>/dev/null");
    Logger::info("容器总大小: " + result.stdout_data);

    // Top 30 最大文件/目录
    std::string cmd =
        "find \"" + root + "\" -type d "
        "\\( -path \"" + root + "/proc\" -o "
        "-path \"" + root + "/dev\" -o "
        "-path \"" + root + "/sys\" -o "
        "-path \"" + root + "/media/sd\" -o "
        "-path \"" + root + "/media/tf\" \\) -prune -o "
        "-type f -print0 2>/dev/null | "
        "xargs -0 du -sh 2>/dev/null | "
        "sort -rh | head -30";

    auto top = Executor::shell(cmd);
    if (!top.stdout_data.empty() && top.stdout_data != "\n") {
        Logger::info("\n📊 Top 30 占用最大文件:");
        Logger::info(top.stdout_data);
    }

    // 各分类缓存大小
    std::vector<std::pair<std::string, std::string>> cache_dirs = {
        {"apt 缓存", root + "/var/cache/apt"},
        {"pacman 缓存", root + "/var/cache/pacman"},
        {"dnf 缓存", root + "/var/cache/dnf"},
        {"systemd 日志", root + "/var/log/journal"},
        {"系统日志", root + "/var/log"},
        {"临时文件", root + "/tmp"},
        {"用户缓存", root + "/root/.cache"},
    };

    Logger::info("\n📊 分类缓存占用:");
    for (const auto& [label, path] : cache_dirs) {
        if (fs::exists(path)) {
            auto r = Executor::shell("du -sh \"" + path + "\" 2>/dev/null | cut -f1");
            std::string size = r.stdout_data;
            size.erase(std::remove(size.begin(), size.end(), '\n'), size.end());
            if (!size.empty() && size != "0") {
                Logger::info("  " + label + ": " + size);
            }
        }
    }
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
                auto size_human = get_archive_size_human((cfg_.backup_dir / latest).string());
                Logger::info("检测到最新备份: " + latest + " (" + size_human + ")");
                if (Logger::confirm("使用此备份还原?")) {
                    archive = (cfg_.backup_dir / latest).string();
                } else {
                    archive = tui_select_file(cfg_.backup_dir.string());
                }
            } else {
                archive = tui_select_file(cfg_.backup_dir.string());
            }
            break;
        }
        case RestoreMode::FromSD: {
            std::vector<std::string> sd_candidates = {
                "/sdcard/tmoe-backup", "/storage/emulated/0/tmoe-backup",
                "/media/sd/tmoe-backup"
            };
            std::string sd_backup;
            for (const auto& c : sd_candidates) {
                if (fs::exists(c)) { sd_backup = c; break; }
            }
            if (sd_backup.empty()) {
                Logger::error("外置存储备份目录不存在");
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
            std::vector<std::string> dl_candidates = {
                "/sdcard/Download/backup",
                "/sdcard/Download/backup/termux",
                "/storage/emulated/0/Download/backup"
            };
            std::string dl_path;
            for (const auto& c : dl_candidates) {
                if (fs::exists(c)) { dl_path = c; break; }
            }
            if (dl_path.empty()) {
                Logger::error("下载备份目录不存在");
                return false;
            }
            archive = tui_select_file(dl_path);
            break;
        }
        case RestoreMode::Compat: {
            // 兼容模式：直接解压到 /，不检查路径
            auto latest = detect_latest_backup();
            if (!latest.empty()) {
                archive = (cfg_.backup_dir / latest).string();
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

    auto size_human = get_archive_size_human(archive);
    Logger::info("备份文件: " + archive + " (" + size_human + ")");

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

    // 确保目标目录存在
    if (!fs::exists(target)) {
        fs::create_directories(target);
    }

    // 检测是否为 chroot 还原（根据文件名）
    bool is_chroot_restore =
        archive.find("_chroot") != std::string::npos;

    if (is_chroot_restore && mode != RestoreMode::Compat) {
        // 清除旧的 chroot 容器标记
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
        std::string chroot_flag = home + "/.config/tmoe-linux/chroot_container";
        if (fs::exists(chroot_flag)) {
            fs::remove(chroot_flag);
            Logger::info("  已清除旧的 chroot 容器标记");
        }
    }

    bool use_progress = has_pv() && !is_chroot_restore && mode != RestoreMode::Compat;
    if (use_progress) {
        Logger::info("正在还原（显示进度）: " + archive + " → " + target);
    } else {
        Logger::info("正在还原: " + archive + " → " + target);
    }

    bool ok = uncompress_archive(archive, target, use_progress);

    if (ok) {
        Logger::ok("还原完成！容器已恢复至: " + target);
    } else {
        Logger::error("还原失败！请检查磁盘空间和备份文件完整性。");
    }
    return ok;
}

bool BackupManager::uncompress_archive(std::string_view archive_path,
                                         std::string_view target_path,
                                         bool show_progress) {
    ArchiveFormat fmt = detect_format(archive_path);
    std::string tar_prefix = get_tar_prefix();
    std::string tar_cmd;

    if (show_progress && has_pv()) {
        // 使用 pv 显示进度
        switch (fmt) {
            case ArchiveFormat::TarZst:
                tar_cmd = tar_prefix + "zstd -d -c \"" + std::string(archive_path) +
                          "\" | pv -p --timer --rate --bytes | " +
                          tar_prefix + "tar -Ppxf - -C \"" +
                          std::string(target_path) + "\"";
                break;
            case ArchiveFormat::TarXz:
                tar_cmd = tar_prefix + "xz -d -c \"" + std::string(archive_path) +
                          "\" | pv -p --timer --rate --bytes | " +
                          tar_prefix + "tar -Ppxf - -C \"" +
                          std::string(target_path) + "\"";
                break;
            case ArchiveFormat::TarGz:
                tar_cmd = "pv \"" + std::string(archive_path) + "\" | " +
                          tar_prefix + "tar -Ppzxf - -C \"" +
                          std::string(target_path) + "\"";
                break;
            default:
                // 通用格式：tar 自动检测
                tar_cmd = "pv \"" + std::string(archive_path) + "\" | " +
                          tar_prefix + "tar -Ppxf - -C \"" +
                          std::string(target_path) + "\"";
                break;
        }
    } else {
        switch (fmt) {
            case ArchiveFormat::TarZst:
                tar_cmd = tar_prefix + "tar --use-compress-program zstd -Ppxf \"" +
                          std::string(archive_path) + "\" -C \"" +
                          std::string(target_path) + "\"";
                break;
            case ArchiveFormat::TarXz:
                tar_cmd = tar_prefix + "tar -PpJxf \"" + std::string(archive_path) +
                          "\" -C \"" + std::string(target_path) + "\"";
                break;
            case ArchiveFormat::TarGz:
                tar_cmd = tar_prefix + "tar -Ppzxf \"" + std::string(archive_path) +
                          "\" -C \"" + std::string(target_path) + "\"";
                break;
            default:
                // 不支持的格式 → 通用 tar 自动检测
                Logger::warn("未知压缩格式，尝试 tar 自动检测解压...");
                tar_cmd = tar_prefix + "tar -Ppxf \"" + std::string(archive_path) +
                          "\" -C \"" + std::string(target_path) + "\"";
                break;
        }
    }

    Logger::debug("执行: " + tar_cmd);
    return Executor::shell(tar_cmd).ok();
}

// ═══════════════════════════════════════════════════════════════
//  桌面环境检测
// ═══════════════════════════════════════════════════════════════

std::string BackupManager::detect_desktop_environment(std::string_view rootfs_path) {
    std::string root(rootfs_path);
    std::string usr_bin = root + "/usr/bin";

    // 按优先级检测桌面环境
    struct DEEntry {
        std::string binary;
        std::string prefix;
    };

    std::vector<DEEntry> de_list = {
        {"startplasma-x11", "kde"},
        {"startkde", "kde"},
        {"mate-session", "mate"},
        {"xfce4-session", "xfce"},
        {"startlxqt", "lxqt"},
        {"startdde", "deepin"},
        {"startlxde", "lxde"},
        {"gnome-session", "gnome"},
        {"cinnamon-session", "cinnamon"},
        {"budgie-wm", "budgie"},
        {"i3", "i3"},
        {"sway", "sway"},
    };

    for (const auto& [binary, prefix] : de_list) {
        if (fs::exists(usr_bin + "/" + binary)) {
            return prefix;
        }
    }

    return "";
}

std::string BackupManager::detect_container_distro(std::string_view rootfs_path) {
    std::string root(rootfs_path);

    // 尝试 /etc/os-release
    std::string os_release = root + "/etc/os-release";
    if (fs::exists(os_release)) {
        std::ifstream ifs(os_release);
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.find("ID=") == 0) {
                std::string id = line.substr(3);
                // 去除引号
                id.erase(std::remove(id.begin(), id.end(), '"'), id.end());
                id.erase(std::remove(id.begin(), id.end(), '\''), id.end());
                return id;
            }
        }
    }

    // 尝试 /etc/debian_version
    if (fs::exists(root + "/etc/debian_version")) return "debian";
    // 尝试 /etc/arch-release
    if (fs::exists(root + "/etc/arch-release")) return "arch";
    // 尝试 /etc/alpine-release
    if (fs::exists(root + "/etc/alpine-release")) return "alpine";
    // 尝试 /etc/gentoo-release
    if (fs::exists(root + "/etc/gentoo-release")) return "gentoo";

    return "unknown";
}

// ═══════════════════════════════════════════════════════════════
//  附加备份目标收集
// ═══════════════════════════════════════════════════════════════

std::vector<std::string> BackupManager::collect_extra_backup_targets(
    std::string_view container_name) {

    std::vector<std::string> targets;
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";

    // ── tmoe 辅助脚本 ──
    std::vector<std::string> tmoe_scripts = {
        home + "/.local/bin/tmoe",
        "/usr/local/bin/tmoe",
        home + "/.local/bin/startxsdl",
        home + "/.local/bin/startvnc",
        home + "/.local/bin/stopvnc",
        home + "/.local/bin/novnc",
        home + "/.local/bin/startx11vnc",
    };

    for (const auto& script : tmoe_scripts) {
        if (fs::exists(script)) targets.push_back(script);
    }

    // ── tmoe 配置目录 ──
    std::vector<std::string> config_dirs = {
        home + "/.config/tmoe-linux",
        "/usr/local/etc/tmoe-linux",
    };
    for (const auto& dir : config_dirs) {
        if (fs::exists(dir)) targets.push_back(dir);
    }

    // ── 容器菜单脚本 ──
    std::string menu_script = home + "/容器选择菜单.sh";
    if (fs::exists(menu_script)) targets.push_back(menu_script);

    // ── QEMU 用户态库（跨架构模拟必需） ──
    std::vector<std::string> qemu_libs = {
        home + "/tmoe-linux/lib",
        home + "/tmoe-linux/lib32",
    };
    for (const auto& lib_dir : qemu_libs) {
        if (fs::exists(lib_dir)) targets.push_back(lib_dir);
    }

    return targets;
}

// ═══════════════════════════════════════════════════════════════
//  Timeshift
// ═══════════════════════════════════════════════════════════════

bool BackupManager::install_timeshift() {
    Logger::step("安装 Timeshift 系统快照工具...");

    std::string install_cmd;

    // 根据宿主机发行版选择包管理器
    if (Executor::has("apt")) {
        install_cmd = "sudo apt update && sudo apt install -y timeshift";
    } else if (Executor::has("pacman")) {
        install_cmd = "sudo pacman -Syu --noconfirm --needed timeshift";
    } else if (Executor::has("dnf")) {
        install_cmd = "sudo dnf install -y timeshift";
    } else if (Executor::has("yum")) {
        install_cmd = "sudo yum install -y timeshift";
    } else if (Executor::has("zypper")) {
        install_cmd = "sudo zypper install -y timeshift";
    } else if (Executor::has("apk")) {
        Logger::warn("Alpine Linux 需要从 edge 源安装 timeshift");
        install_cmd = "sudo apk add timeshift";
    } else {
        Logger::warn("无法检测到支持的包管理器，请手动安装 Timeshift。");
        Logger::info("Debian/Ubuntu: sudo apt install timeshift");
        Logger::info("Arch/Manjaro: sudo pacman -S timeshift");
        Logger::info("Fedora:       sudo dnf install timeshift");
        return false;
    }

    if (!Executor::shell(install_cmd).ok()) {
        Logger::error("Timeshift 安装失败");
        return false;
    }

    Logger::ok("Timeshift 安装完成！");
    return true;
}

bool BackupManager::run_timeshift_backup() {
    if (!Executor::has("timeshift")) {
        Logger::warn("Timeshift 未安装。");
        if (Logger::confirm("是否现在安装 Timeshift?")) {
            if (!install_timeshift()) return false;
        } else {
            return false;
        }
    }

    Logger::step("启动 Timeshift 系统快照...");
    Logger::info("Timeshift 将打开图形界面，请在弹出的窗口中操作。");
    Logger::info("支持 Btrfs 子卷快照和 rsync 增量备份两种模式。");

    if (cfg_.is_root) {
        Executor::shell("timeshift-launcher &");
    } else {
        Executor::shell("sudo timeshift-launcher &");
    }

    Logger::ok("Timeshift 已启动");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  容器移除前备份提示
// ═══════════════════════════════════════════════════════════════

bool BackupManager::prompt_backup_before_removal(std::string_view container_name,
                                                   std::string_view rootfs_path) {
    Logger::warn("⚠️ 建议在移除前进行备份！");
    Logger::warn("若因操作不当而导致数据丢失，开发者概不负责！");

    if (Logger::confirm("是否在移除前备份容器 \"" + std::string(container_name) + "\"?")) {
        return backup_container(container_name, rootfs_path);
    }

    if (!Logger::confirm("确定不移除前备份? (输入 NO 取消整个移除操作)")) {
        return false;  // 用户取消移除
    }

    return true;  // 跳过备份但继续移除
}

// ═══════════════════════════════════════════════════════════════
//  辅助函数
// ═══════════════════════════════════════════════════════════════

bool BackupManager::run_tar_backup(std::string_view source_dir,
                                     std::string_view output_file,
                                     ArchiveFormat format,
                                     const std::vector<std::string>& excludes,
                                     const std::vector<std::string>& includes) {
    // 确保备份目录存在
    fs::path out(output_file);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::ostringstream cmd;
    std::string tar_prefix = get_tar_prefix();

    switch (format) {
        case ArchiveFormat::TarZst:
            cmd << tar_prefix << "tar --use-compress-program zstd -Ppcvf \""
                << output_file << "\"";
            break;
        case ArchiveFormat::TarXz:
            cmd << tar_prefix << "tar -PJpcvf \"" << output_file << "\"";
            break;
        case ArchiveFormat::TarGz:
            cmd << tar_prefix << "tar -Ppzcvf \"" << output_file << "\"";
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

    // 附加 include 路径
    for (const auto& inc : includes) {
        if (fs::exists(inc)) {
            cmd << " \"" << inc << "\"";
        }
    }

    Logger::debug("执行: " + cmd.str());
    return Executor::shell(cmd.str()).ok();
}

bool BackupManager::run_tar_backup_with_progress(std::string_view source_dir,
                                                   std::string_view output_file,
                                                   ArchiveFormat format,
                                                   const std::vector<std::string>& excludes) {
    fs::path out(output_file);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::ostringstream cmd;
    std::string tar_prefix = get_tar_prefix();

    // 构建排除参数
    std::ostringstream exclude_flags;
    for (const auto& ex : excludes) {
        exclude_flags << " --exclude=\"" << ex << "\"";
    }

    switch (format) {
        case ArchiveFormat::TarZst:
            // zstd → pv 管道
            cmd << tar_prefix << "tar --exclude=\""
                << output_file << "\"" << exclude_flags.str()
                << " -Ppcf - \"" << source_dir << "\" | "
                << "pv -p --timer --rate --bytes | "
                << "zstd -T0 > \"" << output_file << "\"";
            break;
        case ArchiveFormat::TarXz:
            cmd << tar_prefix << "tar -PpJcf - \"" << source_dir
                << "\" | pv -p --timer --rate --bytes > \""
                << output_file << "\"";
            break;
        case ArchiveFormat::TarGz:
            cmd << tar_prefix << "tar -Ppzcf - \"" << source_dir
                << "\" | pv -p --timer --rate --bytes > \""
                << output_file << "\"";
            break;
        default:
            return run_tar_backup(source_dir, output_file, format, excludes);
    }

    Logger::debug("执行: " + cmd.str());
    return Executor::shell(cmd.str()).ok();
}

ArchiveFormat BackupManager::tui_select_format() {
    std::string menu = cfg_.tui_bin +
        " --title \"压缩格式\" --menu \"请选择备份压缩格式:\" 0 0 0 "
        "\"1\" \"zstd (推荐 — 速度快，压缩比好)\" "
        "\"2\" \"xz (体积最小，但较慢)\" "
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
        "\"5\" \"🔧 兼容模式 (直接解压到根目录，跳过 pv)\"";

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

std::vector<std::string> BackupManager::tui_multi_select_targets() {
    // 预留：多选备份目标 TUI
    return {};
}

bool BackupManager::tui_confirm_cleanup(std::string_view rootfs_path) {
    std::string cmd = cfg_.tui_bin +
        " --title \"垃圾清理确认\" --yesno \"将清理以下容器的垃圾文件:\\\\n\\\\n"
        "- 包管理器缓存 (apt/pacman/dnf/apk)\\\\n"
        "- systemd journal 日志\\\\n"
        "- VNC 密码 / Xauthority\\\\n"
        "- Shell 历史 (bash/zsh)\\\\n"
        "- 浏览器缓存\\\\n"
        "- pip/npm/cargo/go 缓存\\\\n\\\\n"
        "确认执行?\" 0 0";
    return Executor::shell(cmd).ok();
}

bool BackupManager::is_chroot_container(std::string_view rootfs_path) const {
    std::string root(rootfs_path);

    // 检查 chroot 标记文件
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/root";
    if (fs::exists(home + "/.config/tmoe-linux/chroot_container")) return true;

    // 检查是否是完整系统根目录结构
    if (fs::exists(root + "/etc/fstab") &&
        fs::exists(root + "/boot") &&
        fs::exists(root + "/dev")) {
        return !cfg_.is_termux;  // Termux 环境下不可能是 chroot
    }

    return false;
}

std::string BackupManager::get_tar_prefix() const {
    // 如果是 root 用户运行，不需要 sudo
    if (cfg_.is_root) return "";
    return "sudo ";
}

bool BackupManager::has_pv() const {
    return Executor::has("pv");
}

std::string BackupManager::detect_package_manager(std::string_view rootfs_path) {
    std::string root(rootfs_path);

    // 按优先级检测包管理器
    if (fs::exists(root + "/usr/bin/apt") || fs::exists(root + "/usr/bin/apt-get"))
        return "apt";
    if (fs::exists(root + "/usr/bin/pacman"))
        return "pacman";
    if (fs::exists(root + "/usr/bin/dnf"))
        return "dnf";
    if (fs::exists(root + "/usr/bin/yum"))
        return "yum";
    if (fs::exists(root + "/sbin/apk"))
        return "apk";
    if (fs::exists(root + "/usr/bin/zypper"))
        return "zypper";
    if (fs::exists(root + "/usr/bin/emerge"))
        return "emerge";
    if (fs::exists(root + "/usr/bin/xbps-install"))
        return "xbps";

    return "unknown";
}

std::vector<std::string> BackupManager::list_backups() const {
    std::vector<std::string> result;
    std::string backup_dir = cfg_.backup_dir.string();

    if (!fs::exists(backup_dir)) return result;

    for (const auto& entry : fs::directory_iterator(backup_dir)) {
        if (!entry.is_regular_file()) continue;
        auto name = entry.path().filename().string();
        // 匹配 .zst / .xz / .gz 结尾
        if (name.find(".tar.zst") != std::string::npos ||
            name.find(".tar.xz") != std::string::npos ||
            name.find(".tar.gz") != std::string::npos ||
            name.find(".tgz") != std::string::npos) {
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

std::vector<std::tuple<std::string, int64_t, std::string>>
BackupManager::list_backups_detailed() const {
    std::vector<std::tuple<std::string, int64_t, std::string>> result;
    std::string backup_dir = cfg_.backup_dir.string();

    if (!fs::exists(backup_dir)) return result;

    for (const auto& entry : fs::directory_iterator(backup_dir)) {
        if (!entry.is_regular_file()) continue;
        auto name = entry.path().filename().string();
        if (name.find(".tar.zst") != std::string::npos ||
            name.find(".tar.xz") != std::string::npos ||
            name.find(".tar.gz") != std::string::npos ||
            name.find(".tgz") != std::string::npos) {

            int64_t size_mb = 0;
            try {
                size_mb = entry.file_size() / (1024 * 1024);
            } catch (...) {}

            auto ftime = fs::last_write_time(entry);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
#else
            localtime_r(&time_t, &tm_buf);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M");

            result.emplace_back(name, size_mb, oss.str());
        }
    }

    // 按修改时间降序
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) {
                  return std::get<2>(a) > std::get<2>(b);
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
    // .tar 无压缩、.tar.bz2、.tar.lzma → Unknown，由通用 tar 处理
    if (p.find(".tar") != std::string::npos) return ArchiveFormat::Unknown;
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

    // 容器名 + 桌面环境
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

std::string BackupManager::get_archive_size_human(std::string_view path) {
    try {
        auto size = fs::file_size(path);
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_idx = 0;
        double dsize = static_cast<double>(size);
        while (dsize >= 1024.0 && unit_idx < 4) {
            dsize /= 1024.0;
            unit_idx++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << dsize << " " << units[unit_idx];
        return oss.str();
    } catch (...) {
        return "0 B";
    }
}

} // namespace tmoe::domain
