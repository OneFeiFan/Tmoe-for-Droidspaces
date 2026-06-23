#include "manager.h"

namespace tmoe::app {
    Manager::Manager(TmoeConfig cfg, LocaleSaveFunc save_locale)
        : cfg_(std::move(cfg)), save_locale_(std::move(save_locale)) {
        environment_ = std::make_unique<domain::Environment>(cfg_);
        container_mgr_ = std::make_unique<domain::ContainerManager>(cfg_);
        termux_ = std::make_unique<domain::TermuxManager>(cfg_);
        gui_ = std::make_unique<domain::GUIManager>(cfg_);
        mirror_mgr_ = std::make_unique<domain::MirrorManager>(cfg_);
        backup_mgr_ = std::make_unique<domain::BackupManager>(cfg_);
        docker_mgr_ = std::make_unique<domain::DockerManager>(cfg_);
        virt_mgr_ = std::make_unique<domain::VirtualizationManager>(cfg_);
        virt_mgr_->set_docker_callback([this]() { docker_mgr_->run_docker_menu(); });
        config_mgr_ = std::make_unique<domain::ConfigManager>(cfg_);
        software_center_ = std::make_unique<domain::SoftwareCenter>(cfg_);
        software_center_->set_browser_cb([this]() { browser_->run_browser_menu(); });
        software_center_->set_dev_cb([this]() { dev_tools_->run_dev_tools_menu(); });
        software_center_->set_office_cb([this]() { office_->run_office_menu(); });
        software_center_->set_download_cb([this]() { download_tools_->run_download_menu(); });
        software_center_->set_zsh_cb([this]() { termux_->start_tmoe_zsh(); });
        terminal_app_ = std::make_unique<domain::TerminalAppManager>(cfg_);
        office_ = std::make_unique<domain::OfficeManager>(cfg_);
        education_ = std::make_unique<domain::EducationManager>(cfg_);
        input_method_ = std::make_unique<domain::InputMethodManager>(cfg_);
        browser_ = std::make_unique<domain::BrowserManager>(cfg_);
        beta_features_ = std::make_unique<domain::BetaFeaturesManager>(cfg_);
        // 注入回调：beta_features → 各领域模块
        beta_features_->set_virt_callback([this]() { virt_mgr_->run_virt_menu(); });
        beta_features_->set_education_callback([this]() { education_->run_education_menu(); });
        beta_features_->set_input_method_callback([this]() { input_method_->run_input_method_menu(); });
        beta_features_->set_terminal_callback([this]() { terminal_app_->run_terminal_menu(); });
        dev_tools_ = std::make_unique<domain::DeveloperTools>(cfg_);
        download_tools_ = std::make_unique<domain::DownloadTools>(cfg_);
        init_routes();
    }

    void Manager::init_routes() {
        // 1. Proot 容器向导
        tui_routes_["1"] = [this]() {
            termux_->check_and_init_environment();
            Logger::step(_("container.entering_proot"));
            auto container = container_mgr_->create("debian_default", "debian", "sid");

            if (!container_mgr_->find("debian_default").has_value()) {
                if (Logger::confirm(_("container.confirm_install_default"))) {
                    container.install();
                }
            } else {
                container.start();
            }
            Logger::press_enter();
        };

        // 2. Chroot (需要 root 权限)
        tui_routes_["2"] = [this]() {
            if (!cfg_.is_root) {
                Logger::error(_("error.no_root"));
                return;
            }
            Logger::step(_("container.launching_chroot"));
            // TODO: 实现 Chroot 容器启动逻辑
            Logger::press_enter();
        };

        // 3. 移除/卸载容器
        tui_routes_["3"] = [this]() {
            Logger::step(_("container.entering_remove"));
            auto containers = container_mgr_->list_all();
            if (containers.empty()) {
                Logger::warn(_("container.none_installed"));
                return;
            }

            Logger::info(_("container.list_header"));
            for (const auto &c: containers) {
                Logger::info(" - " + c.name() + " (" + c.distro() + ")");
            }
            // TODO: 扩展 whiptail 多选清单功能
            if (Logger::confirm(_("container.confirm_remove_default"))) {
                container_mgr_->remove("default");
                Logger::ok(_("container.removed_ok"));
            }
            Logger::press_enter();
        };

        // 4. 语言区域切换（支持 zh_CN / en_US）
        tui_routes_["4"] = [this]() {
            run_locale_menu();
        };

        // 5. Termux 额外选项
        tui_routes_["5"] = [this]() {
            termux_->run_termux_menu();
        };

        // 6. 美化终端 (oh-my-zsh 等)
        tui_routes_["6"] = [this]() {
            termux_->beautify_terminal();
            Logger::press_enter();
        };

        // 7. 通过 git pull 自更新
        tui_routes_["7"] = [this]() {
            Logger::step(_("update.fetching"));
            if (Executor::passthrough("git -C " + cfg_.work_dir.string() + " pull").ok()) {
                Logger::ok(_("update.success"));
            } else {
                Logger::error(_("update.failed"));
            }
            Logger::press_enter();
        };

        // 8. 常见问题
        tui_routes_["8"] = [this]() {
            Logger::info("══════════ " + _("faq.title") + " ══════════");
            Logger::info("");
            Logger::info(_("faq.android12"));
            Logger::info("");
            Logger::info(_("faq.no_sound"));
            Logger::info("");
            Logger::info(_("faq.q3"));
            Logger::info("");
            Logger::info(_("faq.q4"));
            Logger::info("");
            Logger::info(_("faq.q5"));
            Logger::press_enter();
        };

        // 9. 反馈 Bug
        tui_routes_["9"] = [this]() {
            Logger::info(_("report.bug_info"));
            Logger::info("https://github.com/2-moe/tmoe-linux");
            Logger::press_enter();
        };

        // 10. 修复 Android 12+ Signal 9
        tui_routes_["10"] = [this]() {
            termux_->fix_android_12_signal_9();
            Logger::press_enter();
        };

        // 11. 镜像源管理子菜单
        tui_routes_["11"] = [this]() {
            run_mirror_menu();
        };

        // 12. 桌面环境安装
        tui_routes_["12"] = [this]() {
            gui_->run_desktop_install_menu();
        };

        // 13. 远程桌面配置 (VNC/x11vnc/XSDL/noVNC/XRDP)
        tui_routes_["13"] = [this]() {
            gui_->run_remote_desktop_menu();
        };

        // 14. 主题美化 (字体/主题/图标/壁纸/光标)
        tui_routes_["14"] = [this]() {
            gui_->run_beautification_menu();
        };

        // 15. 备份/恢复
        tui_routes_["15"] = [this]() {
            backup_mgr_->run_backup_menu();
        };

        // 16. 秘密花园 (容器/虚拟机, 教育, 系统, 商店, 视频, 绘图, 文件, 阅读, 网络, 输入法, 终端, 其他)
        tui_routes_["16"] = [this]() {
            beta_features_->run_beta_menu();
        };

        // 17. 容器配置管理 (DNS/时区/Locale/Fortune/共享目录/密码/主机名)
        tui_routes_["17"] = [this]() {
            config_mgr_->run_config_menu();
        };

        // 18. 软件中心 (社交/游戏/媒体/文档/文件共享/清理)
        tui_routes_["18"] = [this]() {
            software_center_->run_software_center_menu();
        };

    }

    /** 镜像源管理子菜单 (对应原始 Bash 脚本的 12 项 TUI)。 */
    void Manager::run_mirror_menu() {
        using namespace domain;

        while (true) {
            // 第一级：镜像源管理主菜单
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"" + _("mirror.title") + " " + cfg_.os_pretty_name + "\"" +
                                   " --menu \"" + _("mirror.menu_prompt") + " " + cfg_.linux_distro + ")\" 0 0 0 "
                                   "\"1\" \"" + _("mirror.cat_business") + "\" "
                                   "\"2\" \"" + _("mirror.cat_university") + "\" "
                                   "\"3\" \"" + _("mirror.cat_worldwide") + "\" "
                                   "\"4\" \"" + _("mirror.auto_select") + "\" "
                                   "\"5\" \"" + _("mirror.restore_official") + "\" "
                                   "\"6\" \"" + _("mirror.edit_manual") + "\" "
                                   "\"7\" \"" + _("mirror.toggle_protocol") + "\" "
                                   "\"8\" \"" + _("mirror.clean_sources") + "\" "
                                   "\"9\" \"" + _("mirror.trust_sources") + "\" "
                                   "\"A\" \"" + _("mirror.speed_test") + "\" "
                                   "\"B\" \"" + _("mirror.extra_sources") + "\" "
                                   "\"C\" \"" + _("mirror.ping_test") + "\" "
                                   "\"D\" \"" + _("mirror.faq") + "\" "
                                   "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            std::string choice = Executor::tui_select(menu_cmd);
            if (choice == "0" || choice.empty()) break;

            if (choice == "1" || choice == "2" || choice == "3") {
                // 第二级：按分类列出镜像站
                std::string cat;
                if (choice == "1") cat = "business";
                else if (choice == "2") cat = "university";
                else cat = "worldwide";

                auto mirrors = MirrorRegistry::instance().by_category(cat);
                if (choice == "3") {
                    // 全球镜像站：补充非 cn 区域的
                    auto extra = MirrorRegistry::instance().by_region("global");
                    mirrors.insert(mirrors.end(), extra.begin(), extra.end());
                    for (const auto &r: {"hk", "tw", "jp", "kr", "us", "uk", "de", "fr", "ru", "au"}) {
                        auto reg = MirrorRegistry::instance().by_region(r);
                        mirrors.insert(mirrors.end(), reg.begin(), reg.end());
                    }
                }

                if (mirrors.empty()) {
                    Logger::warn(_("mirror.no_available"));
                    Logger::press_enter();
                    continue;
                }

                // 构建 whiptail menu 字符串
                std::string sub_menu = cfg_.tui_bin + " --title \"" + _("mirror.select_mirror_title") + "\" --menu \"" +
                                       _("mirror.select_mirror_prompt") + "\" 0 0 0 ";
                for (size_t i = 0; i < mirrors.size(); ++i) {
                    sub_menu += "\"" + std::to_string(i + 1) + "\" \""
                            + mirrors[i].name + " (" + mirrors[i].url + ")\" ";
                }
                sub_menu += "\"0\" \"" + _("menu.tui.back") + "\"";

                std::string sub_choice = Executor::tui_select(sub_menu);
                if (sub_choice == "0" || sub_choice.empty()) continue;

                int idx = std::stoi(sub_choice) - 1;
                if (idx >= 0 && idx < static_cast<int>(mirrors.size())) {
                    mirror_mgr_->switch_to(mirrors[idx].id);
                }
                Logger::press_enter();
            } else if (choice == "4") {
                mirror_mgr_->auto_select();
                Logger::press_enter();
            } else if (choice == "5") {
                if (Logger::confirm(_("mirror.confirm_restore_official"))) {
                    mirror_mgr_->restore_official();
                }
                Logger::press_enter();
            } else if (choice == "6") {
                mirror_mgr_->edit_manually();
                Logger::press_enter();
            } else if (choice == "7") {
                std::string toggle_cmd = cfg_.tui_bin +
                                         " --title \"" + _("mirror.protocol_title") + "\" --yes-button \"" +
                                         _("mirror.btn_switch_https") + "\" --no-button \"" + _(
                                             "mirror.btn_switch_http") + "\""
                                         " --yesno \"" + _("mirror.select_protocol") + "\" 0 0";
                auto result = Executor::passthrough(toggle_cmd);
                bool use_https = result.exit_code == 0;
                mirror_mgr_->toggle_http_https(use_https);
                Logger::press_enter();
            } else if (choice == "8") {
                if (Logger::confirm(_("mirror.confirm_clean_sources"))) {
                    mirror_mgr_->clean_sources_list();
                }
                Logger::press_enter();
            } else if (choice == "9") {
                std::string trust_cmd = cfg_.tui_bin +
                                        " --title \"" + _("mirror.trust_title") + "\" --yes-button \"" +
                                        _("mirror.btn_trust") + "\" --no-button \"" + _("mirror.btn_untrust") + "\""
                                        " --yesno \"" + _("mirror.trust_warning") + "\" 0 0";
                auto result = Executor::passthrough(trust_cmd);
                bool trust = result.exit_code == 0;
                mirror_mgr_->trust_sources(trust);
                Logger::press_enter();
            } else if (choice == "A") {
                mirror_mgr_->run_download_speed_test();
                Logger::press_enter();
            } else if (choice == "B") {
                mirror_mgr_->manage_extra_sources();
                Logger::press_enter();
            } else if (choice == "C") {
                mirror_mgr_->run_ping_latency_test();
                Logger::press_enter();
            } else if (choice == "D") {
                mirror_mgr_->show_mirror_faq();
                Logger::press_enter();
            }
        }
    }

    std::string Manager::render_and_get_choice() {
        std::string cur_lang = std::string(I18n::current_lang());
        std::string title = _("menu.tui.title_bar") + " " + cfg_.os_pretty_name;

        std::string menu_cmd = "LANG=" + cur_lang + ".UTF-8 " + cfg_.tui_bin +
                               " --title \"" + title + "\" --menu \"" + _("menu.tui.title") + "\" 0 0 0 ";

        menu_cmd += "\"1\" \"" + _("menu.tui.proot") + "\" "
                "\"2\" \"" + _("menu.tui.chroot") + "\" "
                "\"3\" \"" + _("menu.tui.remove") + "\" "
                "\"4\" \"" + _("menu.tui.locale") + "\" "
                "\"5\" \"" + _("menu.tui.termux") + "\" "
                "\"6\" \"" + _("menu.tui.zsh") + "\" "
                "\"7\" \"" + _("menu.tui.update") + "\" "
                "\"8\" \"" + _("menu.tui.faq") + "\" "
                "\"9\" \"" + _("menu.tui.report") + "\" "
                "\"10\" \"" + _("menu.tui.fix_signal9") + "\" "
                "\"11\" \"" + _("menu.tui.mirrors") + "\" "
                "\"12\" \"" + _("menu.tui.gui_de") + "\" "
                "\"13\" \"" + _("menu.tui.gui_remote") + "\" "
                "\"14\" \"" + _("menu.tui.gui_beautify") + "\" "
                "\"15\" \"" + _("menu.tui.backup") + "\" "
                "\"16\" \"" + _("menu.tui.secret_garden") + "\" "
                "\"17\" \"" + _("menu.tui.config") + "\" "
                "\"18\" \"" + _("menu.tui.software_center") + "\" "
                "\"0\" \"" + _("menu.tui.exit") + "\"";

        return Executor::tui_select(menu_cmd);
    }

    int Manager::run_interactive() {
        Logger::info(_("welcome"));
        environment_->initialize();

        // 自动安装宿主机依赖 (此时已以 root 运行)
        environment_->check_dependencies();

        // 应用 Android TUI 兼容补丁并更新 tui_bin 路径
        if (cfg_.is_termux) {
            cfg_.tui_bin = termux_->check_and_patch_tui_env();
        }

        // 首次启动：让用户选择语言
        first_run_locale_setup();

        // 环境前置检查 (配色方案、字体、崩溃修复)
        termux_->check_and_init_environment();

        while (true) {
            std::string choice = render_and_get_choice();

            if (choice == "0" || choice.empty()) {
                break;
            }

            // 通过路由表分发
            auto it = tui_routes_.find(choice);
            if (it != tui_routes_.end()) {
                it->second();
                // press_enter 由各路由内部自行处理 — 避免子菜单返回后双重回车
            } else {
                Logger::warn(_("menu.unimplemented") + ": " + choice);
                Logger::press_enter();
            }
        }

        Logger::info(_("goodbye"));
        return 0;
    }

    /** 直接从命令行参数执行领域逻辑。 */
    int Manager::run_launch_context(const LaunchContext &ctx) {
        if (ctx.mode == LaunchMode::Help) {
            Logger::info("══════════ " + _("help.title") + " ══════════");
            Logger::info("");
            Logger::info(_("help.usage") + ":  tmoe [mode] [distro] [version] [arch] [program] [script]");
            Logger::info("");
            Logger::info(_("help.commands") + ":");
            Logger::info("  proot             " + _("help.proot_cmd"));
            Logger::info("  chroot            " + _("help.chroot_cmd"));
            Logger::info("  nspawn            " + _("help.nspawn_cmd"));
            Logger::info("  ls                " + _("help.ls_cmd"));
            Logger::info("  zsh               " + _("help.zsh_cmd"));
            Logger::info("  theme             " + _("help.theme_cmd"));
            Logger::info("  aria2             " + _("help.aria2_cmd"));
            Logger::info("  i / -i            " + _("help.debian_cmd"));
            Logger::info("  m / manager       " + _("help.manager_cmd"));
            Logger::info("  t / tool          " + _("help.tool_cmd"));
            Logger::info("  -m / mirror       " + _("help.mirror_cmd"));
            Logger::info("  -novnc / -n       " + _("help.novnc_cmd"));
            Logger::info("  -v / -vnc         " + _("help.vnc_cmd"));
            Logger::info("  -s / -stop        " + _("help.stopvnc_cmd"));
            Logger::info("  -x / -xsdl        " + _("help.xsdl_cmd"));
            Logger::info("");
            Logger::info(_("help.examples") + ":");
            Logger::info("  tmoe proot debian sid                     # " + _("help.ex_proot"));
            Logger::info("  tmoe chroot ubuntu focal                  # " + _("help.ex_chroot"));
            Logger::info("  tmoe i                                    # " + _("help.ex_debian"));
            Logger::info("  tmoe -vnc                                 # " + _("help.ex_vnc"));
            Logger::info("  tmoe m                                    # " + _("help.ex_manager"));
            return 0;
        }

        environment_->initialize();

        switch (ctx.mode) {
            case LaunchMode::Proot:
                return launch_container(ctx, domain::ContainerMode::Proot, "PRoot", false);
            case LaunchMode::Chroot:
                return launch_container(ctx, domain::ContainerMode::Chroot, "Chroot", true);
            case LaunchMode::Nspawn:
                return launch_container(ctx, domain::ContainerMode::Nspawn, "systemd-nspawn", true);

            case LaunchMode::ListContainers: {
                auto list = container_mgr_->list_all();
                Logger::info(_("container.list_title"));
                for (const auto &c: list) {
                    std::fprintf(stderr, "  - %s [%s]\n", c.name().c_str(), c.rootfs_path().c_str());
                }
                break;
            }
            case LaunchMode::ZshManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::ThemeManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::MirrorManager:
                run_mirror_menu();
                break;

            case LaunchMode::DebianInstall: {
                Logger::step(_("debian.oneclick_title"));
                Logger::info(_("debian.oneclick_installing"));
                auto container = container_mgr_->create("debian_default", "debian", "sid",
                                                        domain::ContainerMode::Proot);
                if (!container_mgr_->find("debian_default").has_value()) {
                    container.install();
                } else {
                    Logger::warn(_("debian.already_installed"));
                }
                Logger::ok(_("debian.install_complete"));
                break;
            }

            case LaunchMode::ManagerMenu:
                return run_interactive();

            case LaunchMode::ToolMenu:
                dev_tools_->run_dev_tools_menu();
                return 0;

            case LaunchMode::Aria2Manager:
                return download_tools_->run_download_menu(), 0;

            case LaunchMode::GuiManager:
                gui_->handle_gui_cli_flag(ctx.gui_flag);
                return 0;

            case LaunchMode::Help:
            case LaunchMode::Interactive:
            default:
                break;
        }
        return 0;
    }

    int Manager::launch_container(const LaunchContext &ctx, domain::ContainerMode mode,
                                  const std::string &mode_label, bool needs_root) {
        if (needs_root && !cfg_.is_root) {
            Logger::error(_("error.no_root"));
            return 1;
        }

        Logger::info(_f("container.schedule", mode_label, ctx.distro_name));

        std::string container_name = ctx.distro_name.empty() ? "default" : ctx.distro_name;
        std::string distro = ctx.distro_name.empty() ? "debian" : ctx.distro_name;
        auto container = container_mgr_->create(container_name, distro, ctx.distro_code, mode);

        if (!container_mgr_->find(container_name).has_value()) {
            if (Logger::confirm(_("container.confirm_install_default"))) {
                container.install();
            } else {
                return 0;
            }
        }

        // GUI 启动前钩子
        bool is_gui_mode = (ctx.exec_program == "startvnc" || ctx.exec_program == "novnc" ||
                            ctx.exec_program_01 == "startvnc");
        if (is_gui_mode) {
            gui_->start_pulseaudio_bridge();
        }

        if (!container.start(&ctx)) {
            Logger::error(_("container.terminated"));
            return 1;
        }

        // GUI 启动后钩子
        if (is_gui_mode) {
            Executor::shell("sleep 3");
            std::string uri = (ctx.exec_program == "novnc" || ctx.exec_program_01 == "novnc")
                                  ? "http://127.0.0.1:6080"
                                  : "vnc://127.0.0.1:5901";
            environment_->open_uri(uri);
        }
        return 0;
    }

    /** 首次启动语言选择。仅在未持久化过语言偏好时触发。 */
    void Manager::first_run_locale_setup() {
        const char *home = std::getenv("HOME");
        if (!home) return;
        std::string marker = std::string(home) + "/.config/tmoe-linux/locale";
        if (fs::exists(marker)) return; // 已选择过，跳过

        Logger::info(_("locale.firstrun_detected"));
        Logger::info(_("locale.firstrun_detected"));

        // 直接用英文简短提示，此时还没选择语言
        std::string menu = cfg_.tui_bin +
                           " --title \"" + _("locale.firstrun_title") + "\""
                           " --menu \"" + _("locale.firstrun_prompt") + "\" 0 0 0 "
                           "\"en_US\" \"" + _("locale.firstrun.en") + "\" "
                           "\"zh_CN\" \"" + _("locale.firstrun.zh") + "\" ";

        std::string choice = Executor::tui_select(menu);
        if (choice.empty()) return;

        // 验证合法性
        static const std::vector<std::string> valid = {
            "en_US", "zh_CN"
        };
        if (std::find(valid.begin(), valid.end(), choice) == valid.end()) return;

        I18n::init(choice);
        cfg_.locale = choice;
        environment_->set_locale(choice);
        if (save_locale_) save_locale_(choice);
        Logger::ok(_f("locale.set_ok", choice));
        Logger::press_enter();
    }

    /** 语言/区域切换菜单（支持中/英文）。 */
    void Manager::run_locale_menu() {
        // 支持的语言列表: {lang_code, 显示名称, 本地名称}
        struct LangEntry {
            std::string code;
            std::string label;
        };
        const std::vector<LangEntry> LANGS = {
            {"zh_CN", _("locale.zh_cn")},
            {"en_US", _("locale.en_us")},
        };

        // 构建 whiptail 菜单
        std::string cur = std::string(I18n::current_lang());
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"" + _("locale.title") + "\"" +
                               " --menu \"" + _("locale.menu_prompt") + cur +
                               "  |  Select interface language:\" 0 0 0 ";
        for (const auto &e: LANGS) {
            std::string mark = (e.code == cur) ? " ✓" : "";
            menu_cmd += "\"" + e.code + "\" \"" + e.label + mark + "\" ";
        }
        menu_cmd += "\"back\" \"" + _("locale.back") + "\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice.empty() || choice == "back") return;

        // 检查是否是合法代码
        bool valid = false;
        for (const auto &e: LANGS) {
            if (e.code == choice) {
                valid = true;
                break;
            }
        }
        if (!valid) return;

        // 切换语言 (当前进程)
        I18n::init(choice);
        cfg_.locale = choice;
        environment_->set_locale(choice);
        // 持久化 tmoes 自身语言偏好 (下次启动自动加载)
        if (save_locale_) save_locale_(choice);
        Logger::ok(_f("locale.switched_ok", choice));

        // 询问是否持久化为系统默认 locale
        {
            std::string prompt = cfg_.tui_bin +
                                 " --title \"" + _("locale.system_title") + "\""
                                 " --yesno \"" + _("locale.system_yesno") + choice + "。\\n\\n"
                                 "是否同时将此语言设为系统默认 locale？\\n"
                                 "(写入 /etc/default/locale 或 /etc/locale.conf，重启后生效)\" 0 0";
            auto result = Executor::passthrough(prompt);
            if (result.exit_code == 0) {
                environment_->apply_system_locale(choice);
            }
        }

        Logger::press_enter();
    }
} // namespace tmoe::app
