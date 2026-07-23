/** Termux 菜单插件 — 将旧 whiptail 子菜单迁移至新 IUIMenu 框架。 */
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"
#include "domain/system/termux.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/str_utils.h"
#include <sstream>
#include "ui/menus/termux_plugin.h"

namespace tmoe::ui::menus {

// ═══════════════════════════════════════════════════════════════
// GUI 子菜单构建器 — 替换 run_termux_gui_menu()
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_termux_gui_menu() {
        auto menu = make_plugin_menu(
                _("termux.gui_mgmt_title"), _("termux.gui_mgmt_prompt"), "plugin_termux_gui");

        menu->add_action(_("termux.gui_xfce4"), "1",
                         [this] { mgr_->install_termux_xfce(); });

        menu->add_action(_("termux.gui_lxqt"), "2",
                         [this] { mgr_->install_termux_lxqt(); });

        menu->add_action(_("termux.gui_resolution"), "3",
                         [this] { mgr_->configure_termux_vnc(); });

        menu->add_action(_("termux.gui_remove"), "4",
                         [this] { mgr_->remove_termux_gui(); });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// 终端美化子菜单构建器 — 替换 beautify_terminal()
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_termux_beautify_menu() {
        auto menu = make_plugin_menu(
                _("termux.beautify_title"), _("termux.beautify_prompt"), "plugin_termux_beautify");

        menu->add_action(_("termux.beautify_tmoe_zsh"), "1",
                         [this] { mgr_->configure_tmoe_zsh(); });

        menu->add_action(_("termux.beautify_ohmyzsh"), "2",
                         [] {
                             Logger::step(_("termux.installing_ohmyzsh"));
                             domain::PackageManager::install({"zsh", "git", "curl"}, DistroFamily::Debian);
                             Executor::shell(
                                     "sh -c \"$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\" \"\" --unattended");
                             Logger::ok(_("termux.ohmyzsh_installed"));
                         });

        menu->add_action(_("termux.beautify_p10k"), "3",
                         [] {
                             Logger::step(_("termux.installing_p10k"));
                             std::string home = std::getenv("HOME") ? std::getenv("HOME")
                                                                    : "/data/data/com.termux/files/home";
                             std::string p10k_dir = home + "/.oh-my-zsh/custom/themes/powerlevel10k";
                             if (!fs::exists(p10k_dir)) {
                                 Executor::shell(
                                         "git clone --depth=1 https://github.com/romkatv/powerlevel10k.git " +
                                         p10k_dir);
                             }
                             std::string zshrc = home + "/.zshrc";
                             if (fs::exists(zshrc)) {
                                 Executor::shell(
                                         "sudo sed -i 's/^ZSH_THEME=.*/ZSH_THEME=\"powerlevel10k\\/powerlevel10k\"/' " +
                                         zshrc);
                             }
                             Logger::ok(_("termux.p10k_installed"));
                         });

        menu->add_action(_("termux.beautify_colorls"), "4",
                         [] {
                             Logger::step(_("termux.installing_colorls"));
                             domain::PackageManager::install("ruby", DistroFamily::Debian);
                             Executor::shell("gem install colorls");
                             Logger::ok(_("termux.colorls_installed"));
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// 辅助: 仓库切换
// ═══════════════════════════════════════════════════════════════

/** 切换单个 Termux 仓库的启用/禁用状态。
 *  @param repo_name 仓库短名 (root/x11/game/science/unstable) */
    static void toggle_repo(const std::string &repo_name) {
        std::string pkg = repo_name + "-repo";
        auto check = Executor::shell("dpkg -s " + pkg + " 2>/dev/null");

        if (check.ok()) {
            // 已安装 → 禁用
            Logger::step(_f("termux.disabling_repo", repo_name));
            domain::PackageManager::remove(pkg, DistroFamily::Debian);
            domain::PackageManager::update(DistroFamily::Debian);
            Logger::ok(_f("termux.repo_disabled", repo_name));
        } else {
            // 未安装 → 启用
            Logger::step(_f("termux.enabling_repo", repo_name));
            domain::PackageManager::update(DistroFamily::Debian);
            domain::PackageManager::install(pkg, DistroFamily::Debian);
            domain::PackageManager::update(DistroFamily::Debian);
            Logger::ok(_f("termux.repo_enabled", repo_name));
        }
        Logger::press_enter();
    }

// ═══════════════════════════════════════════════════════════════
// Builder: 仓库管理 (原 manage_termux_repos — 5 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_repos_menu() {
        auto menu = make_plugin_menu(
                _("termux.repo_title"), _("termux.repo_prompt"), "plugin_termux_repos");

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.repo_root"), "1",
                [](MenuContext &) -> bool {
                    toggle_repo("root");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.repo_x11"), "2",
                [](MenuContext &) -> bool {
                    toggle_repo("x11");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.repo_game"), "3",
                [](MenuContext &) -> bool {
                    toggle_repo("game");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.repo_science"), "4",
                [](MenuContext &) -> bool {
                    toggle_repo("science");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.repo_unstable"), "5",
                [](MenuContext &) -> bool {
                    toggle_repo("unstable");
                    return true;
                }));

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: Alpine 镜像切换 (原 switch_alpine_mirror — 2 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_alpine_mirror_menu() {
        auto menu = make_plugin_menu(
                _("termux.alpine_mirror_title"), _("termux.alpine_mirror_prompt"), "plugin_termux_alpine");

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.alpine_tuna"), "1",
                [](MenuContext &) -> bool {
                    std::string apk_repos = "/etc/apk/repositories";
                    if (!fs::exists(apk_repos)) {
                        Logger::info(_("termux.alpine_mirror_skip"));
                        Logger::press_enter();
                        return true;
                    }
                    Logger::step(_("termux.alpine_mirror_switching"));
                    Executor::shell("sudo cp " + apk_repos + " " + apk_repos + ".bak 2>/dev/null");
                    Executor::shell(
                            "sudo sed -i 's|http[s]*://[^/]*/alpine|https://mirrors.tuna.tsinghua.edu.cn/alpine|g' "
                            + apk_repos);
                    Logger::ok(_("termux.alpine_mirror_switched_tuna"));
                    Logger::press_enter();
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.alpine_official"), "2",
                [](MenuContext &) -> bool {
                    std::string apk_repos = "/etc/apk/repositories";
                    if (!fs::exists(apk_repos)) {
                        Logger::info(_("termux.alpine_mirror_skip"));
                        Logger::press_enter();
                        return true;
                    }
                    Logger::step(_("termux.alpine_mirror_switching"));
                    Executor::shell(
                            "sudo sed -i 's|http[s]*://[^/]*/alpine|https://dl-cdn.alpinelinux.org/alpine|g' "
                            + apk_repos);
                    Logger::ok(_("termux.alpine_mirror_restored_official"));
                    Logger::press_enter();
                    return true;
                }));

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: 镜像源切换 (原 switch_pkg_mirror — 13 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_mirror_menu() {
        auto menu = make_plugin_menu(
                _("termux.mirror_title"), _("termux.mirror_prompt"), "plugin_termux_mirror");

        // 1. BFSU (北外)
        menu->add_action(_("termux.mirror_bfsu"), "1",
                         [this] {
                             mgr_->modify_termux_sources_list("https://mirrors.bfsu.edu.cn/termux/apt");
                             Logger::ok(_("termux.mirror_switch_ok"));
                         });

        // 2. Tencent (腾讯云)
        menu->add_action(_("termux.mirror_tencent"), "2",
                         [this] {
                             mgr_->modify_termux_sources_list("https://mirrors.cloud.tencent.com/termux/apt");
                             Logger::ok(_("termux.mirror_switch_ok"));
                         });

        // 3. Tsinghua (清华 TUNA)
        menu->add_action(_("termux.mirror_tsinghua"), "3",
                         [this] {
                             mgr_->modify_termux_sources_list("https://mirrors.tuna.tsinghua.edu.cn/termux/apt");
                             Logger::ok(_("termux.mirror_switch_ok"));
                         });

        // 4. USTC (中科大)
        menu->add_action(_("termux.mirror_ustc"), "4",
                         [this] {
                             mgr_->modify_termux_sources_list("https://mirrors.ustc.edu.cn/termux/apt");
                             Logger::ok(_("termux.mirror_switch_ok"));
                         });

        // 5. 仓库管理 → 嵌套子菜单
        menu->add_submenu(_("termux.mirror_repos"), "5", build_repos_menu());

        // 6. 镜像站速度测试
        menu->add_action(_("termux.mirror_speed_test"), "6",
                         [this] { mgr_->run_mirror_speed_test(); });

        // 7. 手动编辑 sources.list
        menu->add_action(_("termux.mirror_edit"), "7",
                         [this] { mgr_->edit_sources_manually(); });

        // 8. 清理 sources.list
        menu->add_action(_("termux.mirror_clean"), "8",
                         [this] { mgr_->clean_sources_list(); });

        // 9. 恢复默认官方源
        menu->add_action(_("termux.mirror_restore"), "9",
                         [this] { mgr_->restore_default_sources(); });

        // A. 备份 sources.list
        menu->add_action(_("termux.mirror_backup_sources"), "A",
                         [this] { mgr_->backup_sources_list(); });

        // B. 旧版镜像格式
        menu->add_action(_("termux.mirror_old_format"), "B",
                         [this] { mgr_->use_old_mirror_format("https://mirrors.tuna.tsinghua.edu.cn/termux"); });

        // C. Alpine 镜像 → 嵌套子菜单
        menu->add_submenu(_("termux.mirror_alpine"), "C", build_alpine_mirror_menu());

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: 磁盘空间查询 (原 check_disk_usage — 4 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_disk_menu() {
        auto menu = make_plugin_menu(
                _("termux.disk_title"), _("termux.disk_prompt"), "plugin_termux_disk");

        // 1. 目录大小排名
        menu->add_action(_("termux.disk_dir_ranking"), "1",
                         [this] { mgr_->show_termux_dir_usage(); });

        // 2. 大文件 TOP30
        menu->add_action(_("termux.disk_large_files"), "2",
                         [this] { mgr_->show_termux_large_files(); });

        // 3. SD 卡占用
        menu->add_action(_("termux.disk_sdcard"), "3",
                         [this] { mgr_->show_sdcard_usage(); });

        // 4. 整体磁盘占用
        menu->add_action(_("termux.disk_overall"), "4",
                         [this] { mgr_->show_overall_disk_usage(); });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: ADB 连接方式 (原 connect_adb_and_fix 内部菜单 — 4 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_adb_connect_menu() {
        auto menu = make_plugin_menu(
                _("termux.adb_connect_title"), _("termux.adb_connect_prompt"), "plugin_termux_adb_connect");

        // 1. 无线配对 (Android 11+)
        menu->add_action(_("termux.adb_wireless_pair"), "1",
                         [this] {
                             mgr_->adb_pair_and_connect_flow();
                             int count = mgr_->count_adb_devices();
                             if (count <= 0) {
                                 Logger::warn(_("termux.adb_no_device"));
                             }
                         });

        // 2. 直接连接 (IP:端口)
        menu->add_action(_("termux.adb_direct"), "2",
                         [] {
                             // 使用 whiptail 输入框获取地址 (保持与旧代码兼容)
                             std::string conn_cmd = std::string("whiptail") +
                                                    " --title \"" + _("termux.adb_addr_title") + "\"" +
                                                    " --inputbox \"" + _("termux.adb_addr_input") + "\" 0 50";
                             std::string adb_target = Executor::tui_select(conn_cmd);
                             if (!adb_target.empty()) {
                                 Logger::step(_f("termux.adb_connecting", adb_target));
                                 Executor::shell("adb connect " + adb_target);
                             }
                         });

        // 3. USB 连接
        menu->add_action(_("termux.adb_usb"), "3",
                         [] { Logger::info(_("termux.adb_usb_hint")); });

        // 4. 列出设备
        menu->add_action(_("termux.adb_list_devices"), "4",
                         [] {
                             Executor::shell("adb devices -l");
                             Logger::info(_("termux.adb_check_device_status"));
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: Signal 9 修复 (原 fix_android_12_signal_9 — 6 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_signal9_menu() {
        auto menu = make_plugin_menu(
                _("termux.signal9_fix_title"), _("termux.signal9_fix_prompt"), "plugin_termux_signal9");

        // 1. ADB 自动修复 (包含连接流程)
        menu->add_action(_("termux.signal9_adb_fix"), "1",
                         [this] { mgr_->connect_adb_and_fix(); });

        // 2. 三星设备兼容模式 + ADB 修复
        menu->add_action(_("termux.signal9_samsung"), "2",
                         [this] {
                             mgr_->set_samsung_adb_comp_mode();
                             mgr_->connect_adb_and_fix();
                         });

        // 3. ADB 配对 + 连接 + 修复
        menu->add_action(_("termux.signal9_adb_pair"), "3",
                         [this] {
                             if (mgr_->adb_pair_and_connect_flow()) {
                                 mgr_->execute_max_phantom_fix("");
                             }
                         });

        // 4. 配置 ADB 服务端口
        menu->add_action(_("termux.signal9_adb_port"), "4",
                         [this] { mgr_->select_adb_port(); });

        // 5. 验证修复结果
        menu->add_action(_("termux.signal9_verify"), "5",
                         [this] { mgr_->verify_signal9_fix(); });

        // 6. 显示手动修复命令
        menu->add_action(_("termux.signal9_manual"), "6",
                         [] {
                             Logger::info(_("termux.signal9_manual_cmd_title"));
                             Logger::info(
                                     "adb shell \"/system/bin/device_config put activity_manager max_phantom_processes 2147483647\"");
                             Logger::info(
                                     "adb shell \"/system/bin/settings put global settings_enable_monitor_phantom_procs false\"");
                             Logger::info(
                                     "adb shell \"/system/bin/device_config set_sync_disabled_for_tests persistent\"");
                         });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: 配色方案 (原 termux_color_scheme_menu — 7 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_color_menu() {
        auto menu = make_plugin_menu(
                _("termux.color_title"), _("termux.color_prompt"), "plugin_termux_color");

        auto download_color = [](const std::string &color_file) {
            Logger::step(_f("termux.color_downloading", color_file));
            std::string url = "https://raw.githubusercontent.com/2cd/zsh/master/share/colors/" + color_file;
            std::string dest =
                    std::string(std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home")
                    + "/.termux/colors.properties";
            CommandBuilder("curl").add_flag("-L").add_flag("-o").add_arg(dest).add_arg(url).execute();
        };

        menu->add_action(_("termux.color_neon"), "1",
                         [download_color] { download_color("neon"); });

        menu->add_action(_("termux.color_monokai"), "2",
                         [download_color] { download_color("monokai.dark"); });

        menu->add_action(_("termux.color_material"), "3",
                         [download_color] { download_color("material"); });

        menu->add_action(_("termux.color_bright"), "4",
                         [download_color] { download_color("bright.light"); });

        menu->add_action(_("termux.color_materia"), "5",
                         [download_color] { download_color("materia"); });

        menu->add_action(_("termux.color_miu"), "6",
                         [download_color] { download_color("miu"); });

        menu->add_action(_("termux.color_wildcherry"), "7",
                         [download_color] { download_color("wild.cherry"); });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// Builder: 字体 (原 termux_font_menu — 6 项)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build_font_menu() {
        auto menu = make_plugin_menu(
                _("termux.font_title"), _("termux.font_prompt"), "plugin_termux_font");

        auto download_font = [](const std::string &font_path) {
            Logger::step(_("termux.font_downloading"));
            std::string termux_dir =
                    std::string(std::getenv("HOME") ? std::getenv("HOME") : "/data/data/com.termux/files/home")
                    + "/.termux";
            std::string url = "https://gitee.com/ak2/" + font_path;
            std::string tar_file = termux_dir + "/font.tar.xz";

            CommandBuilder("curl").add_flag("-L").add_flag("-o").add_arg(tar_file).add_arg(url).execute();
            CommandBuilder("tar").add_flag("-Jxvf").add_arg(tar_file).add_arg("-C").add_arg(termux_dir).execute();
            CommandBuilder("rm").add_flag("-f").add_arg(tar_file).execute();
        };

        menu->add_action(_("termux.font_inconsolata"), "1",
                         [download_font] { download_font("inconsolata-go-font/raw/master/inconsolatago.tar.xz"); });

        menu->add_action(_("termux.font_iosevka"), "2",
                         [download_font] { download_font("inconsolata-go-font/raw/master/iosevka.tar.xz"); });

        menu->add_action(_("termux.font_iosevka_bold"), "3",
                         [download_font] { download_font("iosevka-italic-font/raw/master/font.tar.xz"); });

        menu->add_action(_("termux.font_iosevka_mono"), "4",
                         [download_font] { download_font("iosevka-term-mono/raw/master/font.tar.xz"); });

        menu->add_action(_("termux.font_fira"), "5",
                         [download_font] { download_font("fira-code/raw/master/font.tar.xz"); });

        menu->add_action(_("termux.font_fira_medium"), "6",
                         [download_font] { download_font("fira-code-medium/raw/master/font.tar.xz"); });

        add_sandwich_nav(menu);
        return menu;
    }

// ═══════════════════════════════════════════════════════════════
// 主菜单工厂 — create_termux_menu()
// 嵌套引擎: 项 1(GUI), 4(美化), 5(镜像), 6(磁盘), 8(Signal9)
// ═══════════════════════════════════════════════════════════════

    std::shared_ptr<IUIMenu> TermuxMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("menu.tui.termux"), _("termux.menu_prompt"), "plugin_termux");

        // 1. GUI 子菜单 — 嵌套引擎
        menu->add_submenu(_("termux.gui"), "1", build_termux_gui_menu());

        // 2. 备份
        menu->add_action(_("termux.backup"), "2",
                         [this] { mgr_->backup_termux(); });

        // 3. 还原 — 动态备份文件选择器
        menu->add_child(std::make_shared<LambdaAction>(
                _("termux.restore"), "3",
                [this](MenuContext &ctx) -> bool {
                    std::string backup_dir = "/sdcard/Download/backup/termux";
                    if (!fs::exists(backup_dir)) {
                        Logger::error(_f("termux.backup_dir_not_found", backup_dir));
                        Logger::press_enter();
                        return true;
                    }

                    Logger::step(_("termux.scanning_backups"));
                    std::string ls_result = Executor::shell(
                            "ls -1th " + backup_dir + "/*termux*bak.tar* 2>/dev/null").stdout_data;

                    if (ls_result.empty()) {
                        Logger::error(_("termux.no_backup_found"));
                        Logger::press_enter();
                        return true;
                    }

                    auto file_menu = make_plugin_menu(
                            _("termux.select_backup_title"),
                            _("termux.select_backup_prompt"),
                            "plugin_termux_backup_files");

                    std::stringstream ss(ls_result);
                    std::string line;
                    int idx = 1;

                    while (std::getline(ss, line)) {
                        if (line.empty()) continue;
                        size_t pos = line.find_last_of('/');
                        std::string fname = (pos != std::string::npos) ? line.substr(pos + 1) : line;

                        std::string full_path = line;
                        trim_newline(full_path);

                        file_menu->add_child(std::make_shared<LambdaAction>(
                                fname, std::to_string(idx),
                                [this, full_path](MenuContext &) -> bool {
                                    mgr_->restore_termux(full_path);
                                    Logger::press_enter();
                                    return true;
                                }));
                        idx++;
                    }

                    add_sandwich_nav(file_menu);
                    MenuEngine(ctx).run(file_menu);
                    return true;
                }));

        // 4. 终端美化 — 嵌套引擎
        menu->add_submenu(_("termux.beautify"), "4", build_termux_beautify_menu());

        // 5. 镜像源切换 → 嵌套引擎 (13 项子菜单)
        menu->add_submenu(_("termux.mirror"), "5", build_mirror_menu());

        // 6. 磁盘空间占用 → 嵌套引擎 (4 项子菜单)
        menu->add_submenu(_("termux.disk_usage"), "6", build_disk_menu());

        // 7. 环境初始化
        menu->add_action(_("termux.env_init"), "7",
                         [this] { mgr_->check_and_init_environment(); });

        // 8. Android 12+ Signal 9 修复 → 嵌套引擎 (6 项子菜单)
        menu->add_submenu(_("termux.signal9"), "8", build_signal9_menu());

        // 9. 共享存储设置
        menu->add_action(_("termux.storage"), "9",
                         [this] { mgr_->setup_storage(); });

        // A. PulseAudio 配置
        menu->add_action(_("termux.pulseaudio"), "10",
                         [this] { mgr_->configure_pulseaudio_tcp(); });

        // B. 自更新
        menu->add_action(_("termux.self_update"), "11",
                         [this] { mgr_->self_update(); });

        add_sandwich_nav(menu);
        return menu;
    }

} // namespace tmoe::ui::menus