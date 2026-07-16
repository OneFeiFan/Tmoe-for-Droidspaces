/** Backup 菜单插件 — 将 run_backup_menu() 的每个菜单项拆为独立 LambdaAction。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/system/backup.h"

namespace tmoe::ui::menus {

std::shared_ptr<IUIMenu> create_backup_menu(domain::BackupManager* mgr) {
    auto menu = make_plugin_menu(
        _("menu.tui.backup"), _("backup.menu_prompt"), "plugin_backup");

    // ── 1. 备份容器 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.backup_container"), "backup_container",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            std::string name_cmd = cfg.tui_bin +
                " --title " + _("backup.container_name_title") +
                " --inputbox " + _("backup.container_name_input") + " 0 0 \"debian\"";
            std::string name = Executor::tui_select(name_cmd);
            if (name.empty()) return true;

            std::string path_cmd = cfg.tui_bin +
                " --title " + _("backup.rootfs_path_title") +
                " --inputbox " + _("backup.rootfs_path_input") + " 0 0 \"" +
                (cfg.container_root / name).string() + "\"";
            std::string rootfs = Executor::tui_select(path_cmd);
            if (rootfs.empty()) return true;

            if (fs::exists(rootfs)) {
                mgr->backup_container(name, rootfs);
            } else {
                Logger::error(_f("backup.path_not_found", rootfs));
            }
            Logger::press_enter();
            return true;
        }));

    // ── 2. 恢复容器 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.restore_container"), "restore_container",
        [mgr](MenuContext&) -> bool {
            mgr->restore_container();
            Logger::press_enter();
            return true;
        }));

    // ── 3. 清理垃圾 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.clean_garbage"), "clean_garbage",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            std::string path_cmd = cfg.tui_bin +
                " --title " + _("backup.cleanup_path_title") +
                " --inputbox " + _("backup.cleanup_path_input") + " 0 0 \"" +
                cfg.container_root.string() + "\"";
            std::string rootfs = Executor::tui_select(path_cmd);
            if (rootfs.empty()) return true;

            if (fs::exists(rootfs)) {
                mgr->show_garbage_stats(rootfs);
                if (Logger::confirm(_("backup.confirm_cleanup"))) {
                    mgr->clean_container_garbage(rootfs);
                    Logger::info(_("backup.after_cleanup"));
                    mgr->show_garbage_stats(rootfs);
                }
            }
            Logger::press_enter();
            return true;
        }));

    // ── 4. 备份到外置存储 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.backup_to_sd"), "backup_to_sd",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            std::string name_cmd = cfg.tui_bin +
                " --title " + _("backup.container_name_title") +
                " --inputbox " + _("backup.container_name_input_simple") + " 0 0 \"debian\"";
            std::string name = Executor::tui_select(name_cmd);
            if (name.empty()) return true;

            mgr->backup_to_external_storage(name, (cfg.container_root / name).string());
            Logger::press_enter();
            return true;
        }));

    // ── 5. 列出备份 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.list_backups"), "list_backups",
        [mgr](MenuContext&) -> bool {
            auto backups = mgr->list_backups_detailed();
            if (backups.empty()) {
                Logger::info(_("backup.none_found"));
            } else {
                Logger::info(_("backup.file_list_header") + mgr->config().backup_dir.string() + "):");
                for (const auto& [name, size_mb, mtime] : backups) {
                    Logger::info("  📄 " + name + "  [" + std::to_string(size_mb) + " MB]  " + mtime);
                }
            }
            Logger::press_enter();
            return true;
        }));

    // ── 6. 空间占用排行 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.space_ranking"), "space_ranking",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            std::string path_cmd = cfg.tui_bin +
                " --title " + _("backup.cleanup_path_title") +
                " --inputbox " + _("backup.cleanup_path_input") + " 0 0 \"" +
                cfg.container_root.string() + "\"";
            std::string rootfs = Executor::tui_select(path_cmd);
            if (!rootfs.empty() && fs::exists(rootfs)) {
                mgr->show_garbage_stats(rootfs);
            }
            Logger::press_enter();
            return true;
        }));

    // ── 7. Timeshift ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.timeshift"), "timeshift",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            std::string ts_menu = cfg.tui_bin +
                " --title " + _("backup.timeshift_title") +
                " --menu " + _("backup.timeshift_prompt") + " 0 0 0 "
                "\"1\" " + _("backup.timeshift_create") + " "
                "\"2\" " + _("backup.timeshift_install") + " "
                "\"0\" " + _("menu.tui.back");
            std::string ts_choice = Executor::tui_select(ts_menu);
            if (ts_choice == "1") {
                mgr->run_timeshift_backup();
            } else if (ts_choice == "2") {
                mgr->install_timeshift();
            }
            Logger::press_enter();
            return true;
        }));

    // ── 8. Termux 宿主备份 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.termux_backup"), "termux_backup",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();

            if (!cfg.is_termux) {
                Logger::warn(_("backup.termux_only"));
                Logger::press_enter();
                return true;
            }

            std::string tb_cmd = cfg.tui_bin +
                " --title " + _("backup.tb_backup_title") +
                " --checklist " + _("backup.tb_backup_prompt") + " 0 0 0 "
                "\"home\" " + _("backup.tb_home") + " ON "
                "\"usr\" " + _("backup.tb_usr") + " OFF";
            std::string checklist_result = Executor::tui_select(tb_cmd);
            bool backup_home = checklist_result.find("\"home\"") != std::string::npos;
            bool backup_usr = checklist_result.find("\"usr\"") != std::string::npos;

            if (!backup_home && !backup_usr) {
                Logger::warn(_("backup.no_items_selected"));
                Logger::press_enter();
                return true;
            }

            std::string termux_prefix = "/data/data/com.termux/files";
            auto fmt = mgr->tui_select_format();
            std::string ext = (fmt == domain::ArchiveFormat::TarZst) ? ".tar.zst" :
                              (fmt == domain::ArchiveFormat::TarXz) ? ".tar.xz" : ".tar.gz";

            fs::path out_dir = fs::path("/sdcard/Download/backup/termux");
            if (!fs::exists(out_dir)) fs::create_directories(out_dir);

            if (backup_home) {
                std::string home_file = "termux-home-" + mgr->generate_filename("home", "", "") + ext;
                Logger::step(_("backup.backing_up_home"));
                mgr->run_tar_backup_with_progress(termux_prefix + "/home",
                                                  (out_dir / home_file).string(), fmt);
            }
            if (backup_usr) {
                std::vector<std::string> excludes = {termux_prefix + "/home/tmoe-linux/containers"};
                std::string usr_file = "termux-usr-" + mgr->generate_filename("usr", "", "") + ext;
                Logger::step(_("backup.backing_up_prefix"));
                mgr->run_tar_backup_with_progress(termux_prefix + "/usr",
                                                  (out_dir / usr_file).string(), fmt, excludes);
            }
            Logger::ok(_("backup.termux_backup_complete") + out_dir.string());
            Logger::press_enter();
            return true;
        }));

    // ── 9. 备份目录大小 ──
    menu->add_child(std::make_shared<LambdaAction>(
        _("backup.backup_dir_size"), "backup_dir_size",
        [mgr](MenuContext&) -> bool {
            auto& cfg = mgr->config();
            std::string backup_dir = cfg.backup_dir.string();

            if (!fs::exists(backup_dir)) {
                Logger::info(_("backup.backup_dir_not_found") + backup_dir);
            } else {
                auto result = Executor::shell("du -sh \"" + backup_dir + "\" 2>/dev/null");
                Logger::info(_("backup.backup_dir_size_info") + backup_dir);
                Logger::info("  " + result.stdout_data);
            }
            Logger::press_enter();
            return true;
        }));

    add_sandwich_nav(menu);
    return menu;
}

} // namespace tmoe::ui::menus
