#include "domain/system/backup.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"
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
//  容器备份
// ═══════════════════════════════════════════════════════════════

bool BackupManager::backup_container(std::string_view container_name,
                                      std::string_view rootfs_path) {
    Logger::step(_f("backup.backing_up_container", std::string(container_name)));

    // 检查 rootfs 路径是否存在
    if (!fs::exists(rootfs_path)) {
        Logger::error(_("backup.rootfs_not_found") + std::string(rootfs_path));
        return false;
    }

    // 检测是否为 chroot 容器
    bool is_chroot = is_chroot_container(rootfs_path);
    if (is_chroot) {
        Logger::info(_("backup.detected_chroot"));
    }

    // 检测桌面环境
    std::string desktop = detect_desktop_environment(rootfs_path);
    if (!desktop.empty()) {
        Logger::info(_("backup.detected_desktop") + desktop);
    }

    // 提示是否先清理垃圾
    bool do_cleanup = Logger::confirm(_("backup.confirm_clean_before_backup"));
    if (do_cleanup) {
        show_garbage_stats(rootfs_path);
        if (Logger::confirm(_("backup.confirm_cleanup_items"))) {
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

    Logger::info(_("backup.backup_output") + output_path.string());

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
        Logger::info(_("backup.packaging_with_progress"));
    } else {
        Logger::info(_("backup.packaging_please_wait"));
    }

    bool ok;
    if (use_progress) {
        ok = run_tar_backup_with_progress(root, output_path.string(), fmt, excludes);
    } else {
        ok = run_tar_backup(root, output_path.string(), fmt, excludes);
    }

    // 备份附加目标（追加到 tar）
    if (ok && !extra_targets.empty()) {
        Logger::step(_("backup.appending_targets"));
        for (const auto& target : extra_targets) {
            if (fs::exists(target)) {
                CommandBuilder cb("tar");
                auto tp = get_tar_prefix();
                while (!tp.empty() && tp.back() == ' ') tp.pop_back();
                if (!tp.empty()) cb.set_prefix(tp);
                cb.add_opt("--use-compress-program", "zstd");
                cb.add_flag("-Prvf").add_arg(output_path.string()).add_arg(target);
                cb.add_raw("2>/dev/null");
                Executor::passthrough(cb.build_string());
            }
        }
    }

    if (ok) {
        auto size_human = get_archive_size_human(output_path.string());
        Logger::ok(_f("backup.complete", output_path.filename().string(), size_human));
        Logger::info(_("backup.backup_location") + output_path.string());
    } else {
        Logger::error(_("backup.failed"));
    }
    return ok;
}

bool BackupManager::backup_to_external_storage(std::string_view container_name,
                                                 std::string_view rootfs_path) {
    Logger::step(_("backup.backing_up_sd") + std::string(container_name));

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
        Logger::error(_("backup.no_external_storage"));
        return false;
    }

    fs::path ext_backup = fs::path(ext_storage) / "tmoe-backup";
    if (!fs::exists(ext_backup)) {
        fs::create_directories(ext_backup);
    }

    ArchiveFormat fmt = tui_select_format();
    std::string desktop = detect_desktop_environment(rootfs_path);

    bool do_cleanup = Logger::confirm(_("backup.confirm_clean_first"));
    if (do_cleanup) clean_container_garbage(rootfs_path);

    std::string filename = generate_filename(std::string(container_name), "", desktop);
    if (fmt == ArchiveFormat::TarZst) filename += ".tar.zst";
    else if (fmt == ArchiveFormat::TarXz) filename += ".tar.xz";
    else filename += ".tar.gz";

    fs::path output_path = ext_backup / filename;
    Logger::info(_("backup.backup_output") + output_path.string());

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
        Logger::ok(_("backup.backup_sd_complete") + output_path.string() + " (" + size_human + ")");
    }
    return ok;
}

// ═══════════════════════════════════════════════════════════════
//  容器清理（增强版：多发行版 + systemd + VNC + ZSH + 应用缓存）
// ═══════════════════════════════════════════════════════════════

bool BackupManager::clean_container_garbage(std::string_view rootfs_path) {
    Logger::step(_("backup.cleaning_garbage"));

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
    Executor::passthrough(sys_cmd.str());

    // ── 4. 临时文件 ──
    std::ostringstream tmp_cmd;
    tmp_cmd << "rm -rf "
            << root << "/tmp/.* "
            << root << "/tmp/* "
            << root << "/var/tmp/* "
            << " 2>/dev/null";
    Executor::passthrough(tmp_cmd.str());

    // ── 5. 用户级垃圾（VNC/ZSH/应用缓存） ──
    clean_user_garbage(rootfs_path);

    Logger::ok(_("backup.garbage_cleaned"));
    return true;
}

bool BackupManager::clean_package_cache(std::string_view rootfs_path) {
    std::string root(rootfs_path);
    std::string pkg_mgr = detect_package_manager(rootfs_path);

    if (pkg_mgr == "apt") {
        // Debian/Ubuntu
        Executor::passthrough("rm -rf " + root + "/var/cache/apt/archives/*.deb "
                        + root + "/var/cache/apt/archives/partial/* "
                        + root + "/var/cache/apt/*.bin "
                        + root + "/var/lib/apt/lists/* "
                        + root + "/var/lib/apt/lists/partial/* "
                        + root + "/var/cache/man/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.apt_cache_cleaned"));
    } else if (pkg_mgr == "pacman") {
        // Arch/Manjaro
        Executor::passthrough("rm -rf " + root + "/var/cache/pacman/pkg/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.pacman_cache_cleaned"));
    } else if (pkg_mgr == "dnf" || pkg_mgr == "yum") {
        // Fedora/RHEL
        Executor::passthrough("rm -rf " + root + "/var/cache/dnf/* "
                        + root + "/var/cache/yum/* "
                        + root + "/var/lib/dnf/yumdb/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.dnf_cache_cleaned"));
    } else if (pkg_mgr == "apk") {
        // Alpine
        Executor::passthrough("rm -rf " + root + "/var/cache/apk/* "
                        + root + "/etc/apk/cache/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.apk_cache_cleaned"));
    } else if (pkg_mgr == "zypper") {
        // openSUSE
        Executor::passthrough("rm -rf " + root + "/var/cache/zypp/packages/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.zypper_cache_cleaned"));
    } else if (pkg_mgr == "emerge") {
        // Gentoo
        Executor::passthrough("rm -rf " + root + "/usr/portage/distfiles/* "
                        + root + "/var/cache/distfiles/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.emerge_cache_cleaned"));
    } else if (pkg_mgr == "xbps") {
        // Void
        Executor::passthrough("rm -rf " + root + "/var/cache/xbps/* "
                        + " 2>/dev/null");
        Logger::info(_("backup.xbps_cache_cleaned"));
    }

    return true;
}

bool BackupManager::clean_systemd_journal(std::string_view rootfs_path) {
    std::string root(rootfs_path);
    std::string journal_dir = root + "/var/log/journal";

    if (fs::exists(journal_dir)) {
        Executor::passthrough("rm -rf \"" + journal_dir + "\"/* 2>/dev/null");
        Logger::info(_("backup.journal_cleaned"));
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

    Executor::passthrough(cmd.str());
    Logger::info(_("backup.user_garbage_cleaned"));

    return true;
}

void BackupManager::show_garbage_stats(std::string_view rootfs_path) {
    std::string root(rootfs_path);

    // 整体大小
    auto result = Executor::shell(CommandBuilder("du").add_flag("-sh").add_arg(root).add_raw("2>/dev/null").build_string());
    Logger::info(_("backup.total_size") + result.stdout_data);

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
        Logger::info(_("backup.top_30_files"));
        Logger::info(top.stdout_data);
    }

    // 各分类缓存大小
    std::vector<std::pair<std::string, std::string>> cache_dirs = {
        {_("backup.cache_apt"), root + "/var/cache/apt"},
        {_("backup.cache_pacman"), root + "/var/cache/pacman"},
        {_("backup.cache_dnf"), root + "/var/cache/dnf"},
        {_("backup.cache_systemd_journal"), root + "/var/log/journal"},
        {_("backup.cache_syslog"), root + "/var/log"},
        {_("backup.cache_tmp"), root + "/tmp"},
        {_("backup.cache_user"), root + "/root/.cache"},
    };

    Logger::info(_("backup.cache_by_category"));
    for (const auto& [label, path] : cache_dirs) {
        if (fs::exists(path)) {
            auto r = Executor::shell(CommandBuilder("du").add_flag("-sh").add_arg(path).add_raw("2>/dev/null | cut -f1").build_string());
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
    Logger::step(_("backup.restoring"));

    RestoreMode mode = tui_select_restore_mode();
    std::string archive;

    switch (mode) {
        case RestoreMode::Normal: {
            // 从默认备份目录恢复
            auto latest = detect_latest_backup();
            if (!latest.empty()) {
                auto size_human = get_archive_size_human((cfg_.backup_dir / latest).string());
                Logger::info(_("backup.detected_latest") + latest + " (" + size_human + ")");
                if (Logger::confirm(_("backup.confirm_restore_from_this"))) {
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
                Logger::error(_("backup.ext_backup_not_found"));
                return false;
            }
            archive = tui_select_file(sd_backup);
            break;
        }
        case RestoreMode::ManualPath: {
            std::string path_cmd = cfg_.tui_bin +
                " --title " + _("backup.select_file_manual_title") + " --inputbox " + _("backup.select_file_manual_input") + " 0 0";
            archive = Executor::tui_select(path_cmd);
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
                Logger::error(_("backup.dl_backup_not_found"));
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
        Logger::warn(_("backup.no_backup_selected"));
        return false;
    }

    if (!fs::exists(archive)) {
        Logger::error(_("backup.backup_file_not_found") + archive);
        return false;
    }

    auto size_human = get_archive_size_human(archive);
    Logger::info(_("backup.backup_file_info") + archive + " (" + size_human + ")");

    // 确认目标路径
    std::string target_cmd = cfg_.tui_bin +
        " --title " + _("backup.restore_target_title") + " --inputbox " + _("backup.restore_target_input") + " 0 0 \"" +
        cfg_.container_root.string() + "\"";
    std::string target = Executor::tui_select(target_cmd);

    if (target.empty()) {
        Logger::error(_("backup.no_target_path"));
        return false;
    }

    // 还原前警告
    Logger::warn(_("backup.restore_warn_overwrite"));
    if (!Logger::confirm(_("backup.confirm_restore"))) {
        Logger::info(_("backup.cancelled"));
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
            Logger::info(_("backup.chroot_flag_cleared"));
        }
    }

    bool use_progress = has_pv() && !is_chroot_restore && mode != RestoreMode::Compat;
    if (use_progress) {
        Logger::info(_("backup.restoring_with_progress") + archive + "  " + target);
    } else {
        Logger::info(_("backup.restoring_info") + archive + "  " + target);
    }

    bool ok = uncompress_archive(archive, target, use_progress);

    if (ok) {
        Logger::ok(_f("backup.restore_complete", target));
    } else {
        Logger::error(_("backup.restore_failed"));
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
                Logger::warn(_("backup.unknown_format"));
                tar_cmd = tar_prefix + "tar -Ppxf \"" + std::string(archive_path) +
                          "\" -C \"" + std::string(target_path) + "\"";
                break;
        }
    }

    Logger::debug("Executing: " + tar_cmd);
    return Executor::passthrough(tar_cmd).ok();
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

    return targets;
}

// ═══════════════════════════════════════════════════════════════
//  Timeshift
// ═══════════════════════════════════════════════════════════════

bool BackupManager::install_timeshift() {
    Logger::step(_("backup.timeshift_installing"));

    auto family = PackageManager::detect_distro_family();

    if (family == DistroFamily::Alpine) {
        Logger::warn(_("backup.alpine_timeshift_hint"));
    }

    if (family == DistroFamily::Unknown) {
        Logger::warn(_("backup.no_pm_detected"));
        Logger::info(_("backup.timeshift_debian_hint"));
        Logger::info(_("backup.timeshift_arch_hint"));
        Logger::info(_("backup.timeshift_fedora_hint"));
        return false;
    }

    // Debian needs index update before install
    if (family == DistroFamily::Debian) {
        PackageManager::update(DistroFamily::Debian);
    }

    if (!PackageManager::install("timeshift", family)) {
        Logger::error(_("backup.timeshift_install_failed"));
        return false;
    }

    Logger::ok(_("backup.timeshift_install_ok"));
    return true;
}

bool BackupManager::run_timeshift_backup() {
    if (!Executor::has("timeshift")) {
        Logger::warn(_("backup.timeshift_not_installed"));
        if (Logger::confirm(_("backup.confirm_install_timeshift"))) {
            if (!install_timeshift()) return false;
        } else {
            return false;
        }
    }

    Logger::step(_("backup.timeshift_starting"));
    Logger::info(_("backup.timeshift_gui_hint"));
    Logger::info(_("backup.timeshift_desc"));

    if (cfg_.is_root) {
        Executor::passthrough("timeshift-launcher &");
    } else {
        Executor::passthrough("sudo timeshift-launcher &");
    }

    Logger::ok(_("backup.timeshift_launched"));
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  容器移除前备份提示
// ═══════════════════════════════════════════════════════════════

bool BackupManager::prompt_backup_before_removal(std::string_view container_name,
                                                   std::string_view rootfs_path) {
    Logger::warn(_("backup.backup_recommended"));
    Logger::warn(_("backup.no_responsibility"));

    if (Logger::confirm(_("backup.confirm_backup_before_remove"))) {
        return backup_container(container_name, rootfs_path);
    }

    if (!Logger::confirm(_("backup.confirm_proceed_without_backup"))) {
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

    std::string tar_prefix = get_tar_prefix();
    CommandBuilder cb("tar");

    if (!tar_prefix.empty()) {
        std::string p = tar_prefix;
        while (!p.empty() && p.back() == ' ') p.pop_back();
        if (!p.empty()) cb.set_prefix(p);
    }

    switch (format) {
        case ArchiveFormat::TarZst:
            cb.add_opt("--use-compress-program", "zstd");
            cb.add_flag("-Ppcvf").add_arg(std::string(output_file));
            break;
        case ArchiveFormat::TarXz:
            cb.add_flag("-PJpcvf").add_arg(std::string(output_file));
            break;
        case ArchiveFormat::TarGz:
            cb.add_flag("-Ppzcvf").add_arg(std::string(output_file));
            break;
        default:
            Logger::error(_("backup.unsupported_format"));
            return false;
    }

    // 排除路径
    for (const auto& ex : excludes) {
        cb.add_kv("--exclude", ex);
    }

    cb.add_arg(std::string(source_dir));

    // 附加 include 路径
    for (const auto& inc : includes) {
        if (fs::exists(inc)) {
            cb.add_arg(inc);
        }
    }

    Logger::debug("Executing: " + cb.build_string());
    return Executor::passthrough(cb.build_string()).ok();
}

bool BackupManager::run_tar_backup_with_progress(std::string_view source_dir,
                                                   std::string_view output_file,
                                                   ArchiveFormat format,
                                                   const std::vector<std::string>& excludes) {
    fs::path out(output_file);
    if (!fs::exists(out.parent_path())) {
        fs::create_directories(out.parent_path());
    }

    std::string tar_prefix = get_tar_prefix();
    CommandBuilder cb("tar");

    if (!tar_prefix.empty()) {
        std::string p = tar_prefix;
        while (!p.empty() && p.back() == ' ') p.pop_back();
        if (!p.empty()) cb.set_prefix(p);
    }

    switch (format) {
        case ArchiveFormat::TarZst:
            cb.add_kv("--exclude", std::string(output_file));
            for (const auto& ex : excludes) cb.add_kv("--exclude", ex);
            cb.add_flag("-Ppcf").add_arg("-").add_arg(std::string(source_dir));
            cb.add_raw("| pv -p --timer --rate --bytes | zstd -T0 > \"" + std::string(output_file) + "\"");
            break;
        case ArchiveFormat::TarXz:
            cb.add_flag("-PpJcf").add_arg("-").add_arg(std::string(source_dir));
            cb.add_raw("| pv -p --timer --rate --bytes > \"" + std::string(output_file) + "\"");
            break;
        case ArchiveFormat::TarGz:
            cb.add_flag("-Ppzcf").add_arg("-").add_arg(std::string(source_dir));
            cb.add_raw("| pv -p --timer --rate --bytes > \"" + std::string(output_file) + "\"");
            break;
        default:
            return run_tar_backup(source_dir, output_file, format, excludes);
    }

    Logger::debug("Executing: " + cb.build_string());
    return Executor::passthrough(cb.build_string()).ok();
}

ArchiveFormat BackupManager::tui_select_format() {
    std::string menu = cfg_.tui_bin +
        " --title " + _("backup.format_title") + " --menu " + _("backup.format_prompt") + " 0 0 0 "
        "\"1\" " + _("backup.format_zstd") + " "
        "\"2\" " + _("backup.format_xz") + " "
        "\"3\" " + _("backup.format_gzip");

    std::string choice = Executor::tui_select(menu);
    if (choice == "2") return ArchiveFormat::TarXz;
    if (choice == "3") return ArchiveFormat::TarGz;
    return ArchiveFormat::TarZst;  // 默认 zstd
}

BackupManager::RestoreMode BackupManager::tui_select_restore_mode() {
    std::string menu = cfg_.tui_bin +
        " --title " + _("backup.restore_mode_title") + " --menu " + _("backup.restore_mode_prompt") + " 0 0 0 "
        "\"1\" " + _("backup.restore_normal") + " "
        "\"2\" " + _("backup.restore_sd") + " "
        "\"3\" " + _("backup.restore_manual") + " "
        "\"4\" " + _("backup.restore_download") + " "
        "\"5\" " + _("backup.restore_compat");

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
        " --title " + _("backup.select_file_title") + " --fselect \"" + base_dir + "/\" 0 0";
    // tui_select 通过 3>&1 1>&2 2>&3 交换捕获 whiptail 的 stderr(fselect路径) 输出
    return Executor::tui_select(cmd);
}

std::vector<std::string> BackupManager::tui_multi_select_targets() {
    // 预留：多选备份目标 TUI
    return {};
}

bool BackupManager::tui_confirm_cleanup(std::string_view rootfs_path) {
    std::string cmd = cfg_.tui_bin +
        " --title " + _("backup.cleanup_confirm_title") + " --yesno \"" + _("backup.confirm_cleanup_text") + "\" 0 0";
    return Executor::passthrough(cmd).ok();
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
