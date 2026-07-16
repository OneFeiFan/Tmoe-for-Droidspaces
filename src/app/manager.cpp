#include "manager.h"
#include "domain/system/package_manager.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "ui/menu_engine.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menus/plugin_factories.h"
#include <algorithm>
#include <fstream>

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
        virt_mgr_->set_docker_callback([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_docker_menu(docker_mgr_.get()));
        });
        config_mgr_ = std::make_unique<domain::ConfigManager>(cfg_);
        software_center_ = std::make_unique<domain::SoftwareCenter>(cfg_);
        software_center_->set_browser_cb([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_browser_menu(browser_.get()));
        });
        software_center_->set_dev_cb([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_dev_tools_menu(dev_tools_.get()));
        });
        software_center_->set_office_cb([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_office_menu(office_.get()));
        });
        software_center_->set_download_cb([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_download_menu(download_tools_.get()));
        });
        software_center_->set_zsh_cb([this]() { termux_->start_tmoe_zsh(); });
        terminal_app_ = std::make_unique<domain::TerminalAppManager>(cfg_);
        office_ = std::make_unique<domain::OfficeManager>(cfg_);
        education_ = std::make_unique<domain::EducationManager>(cfg_);
        input_method_ = std::make_unique<domain::InputMethodManager>(cfg_);
        browser_ = std::make_unique<domain::BrowserManager>(cfg_);
        beta_features_ = std::make_unique<domain::BetaFeaturesManager>(cfg_);
        beta_features_->set_virt_callback([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_virtualization_menu(virt_mgr_.get()));
        });
        beta_features_->set_education_callback([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_education_menu(education_.get()));
        });
        beta_features_->set_input_method_callback([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_input_method_menu(input_method_.get()));
        });
        beta_features_->set_terminal_callback([this]() {
            tmoe::ui::MenuContext ctx{cfg_};
            tmoe::ui::MenuEngine(ctx).run(ui::menus::create_terminal_app_menu(terminal_app_.get()));
        });
        dev_tools_ = std::make_unique<domain::DeveloperTools>(cfg_);
        download_tools_ = std::make_unique<domain::DownloadTools>(cfg_);
        register_plugins();
    }

    // ═══════════════════════════════════════════════════════
    // 容器操作辅助方法（原 tui_routes_ 内联逻辑）
    // ═══════════════════════════════════════════════════════

    void Manager::action_proot_container() {
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
    }

    void Manager::action_chroot_container() {
        if (!cfg_.is_root) {
            Logger::error(_("error.no_root"));
            return;
        }
        Logger::step(_("container.launching_chroot"));
        Logger::press_enter();
    }

    void Manager::action_remove_container() {
        Logger::step(_("container.entering_remove"));
        auto containers = container_mgr_->list_all();
        if (containers.empty()) {
            Logger::warn(_("container.none_installed"));
            return;
        }
        Logger::info(_("container.list_header"));
        for (const auto &c : containers) {
            Logger::info(_f("container.list_item", c.name(), c.distro()));
        }
        if (Logger::confirm(_("container.confirm_remove_default"))) {
            container_mgr_->remove("default");
            Logger::ok(_("container.removed_ok"));
        }
        Logger::press_enter();
    }

    void Manager::action_self_update() {
        Logger::step(_("update.fetching"));
        if (CommandBuilder("git").add_flag("-C").add_arg(cfg_.work_dir.string()).add_arg("pull").execute().ok()) {
            Logger::ok(_("update.success"));
        } else {
            Logger::error(_("update.failed"));
        }
        Logger::press_enter();
    }

    void Manager::action_bug_report() {
        Logger::info(_("report.bug_info"));
        Logger::info("https://github.com/2-moe/tmoe-linux");
        Logger::press_enter();
    }

    void Manager::register_plugins() {
        // 领域模块插件通过旧回调机制在 run_xxx_menu() 内部访问，
        // 不需要注册到 MenuRegistry。
        // 主入口菜单由 build_root_menu() 按旧 UI 结构构建。
    }

    std::shared_ptr<tmoe::ui::IUIMenu> Manager::build_root_menu() {
        using namespace tmoe::ui;

        std::string title = _("menu.tui.title_bar") + " " + cfg_.os_pretty_name;

        if (cfg_.is_termux) {
            // Termux 容器管理：10 项
            auto menu = make_plugin_menu(title, _("menu.tui.manager_prompt"), "main_manager");
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.proot"), "1",
                [this](MenuContext&) -> bool { action_proot_container(); return true; }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.chroot"), "2",
                [this](MenuContext&) -> bool { action_chroot_container(); return true; }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.remove"), "3",
                [this](MenuContext&) -> bool { action_remove_container(); return true; }));
            // 4: 语言切换 — 嵌套引擎运行新式菜单
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.locale"), "4",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(build_locale_menu()); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.termux"), "5",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_termux_menu(termux_.get())); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.zsh"), "6",
                [this](MenuContext&) -> bool { termux_->beautify_terminal(); Logger::press_enter(); return true; }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.update"), "7",
                [this](MenuContext&) -> bool { action_self_update(); return true; }));
            // 8: FAQ — 嵌套引擎运行新式菜单
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.faq"), "8",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(build_faq_menu()); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.report"), "9",
                [this](MenuContext&) -> bool { action_bug_report(); return true; }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.fix_signal9"), "10",
                [this](MenuContext&) -> bool { termux_->fix_android_12_signal_9(); Logger::press_enter(); return true; }));
            add_navigation_items(menu);
            return menu;
        } else {
            // Linux 工具箱：7 项
            auto menu = make_plugin_menu(title, _("menu.tui.tool_prompt"), "main_tool");
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.gui_de"), "1",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_desktop_menu(gui_.get())); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.software_center"), "2",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_software_center_menu(software_center_.get())); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.secret_garden"), "3",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_beta_features_menu(beta_features_.get())); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.gui_beautify"), "4",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_beautify_menu(gui_.get())); return true;
                }));
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.gui_remote"), "5",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(menus::create_remote_desktop_menu(gui_.get())); return true;
                }));
            // 6: 镜像源 — 嵌套引擎运行新式菜单
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.mirrors"), "6",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(build_mirror_menu()); return true;
                }));
            // 7: FAQ — 嵌套引擎运行新式菜单
            menu->add_child(std::make_shared<LambdaAction>(
                _("menu.tui.faq"), "7",
                [this](MenuContext& ctx) -> bool {
                    MenuEngine(ctx).run(build_faq_menu()); return true;
                }));
            add_navigation_items(menu);
            return menu;
        }
    }

    // ═══════════════════════════════════════════════════════
    // 子菜单构建器（逐步替换旧 whiptail 字符串拼接）
    // ═══════════════════════════════════════════════════════

    std::shared_ptr<ui::IUIMenu> Manager::build_faq_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(_("faq.title"), _("faq.menu_prompt"), "faq");

        // 1-2: 仅显示信息
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q1"), "1",
            [this](MenuContext&) -> bool { Logger::info(_("faq.a1")); Logger::press_enter(); return true; }));
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q2"), "2",
            [this](MenuContext&) -> bool { Logger::info(_("faq.a2")); Logger::press_enter(); return true; }));
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q3"), "3",
            [this](MenuContext&) -> bool { Logger::info(_("faq.a3")); Logger::press_enter(); return true; }));
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q4"), "4",
            [this](MenuContext&) -> bool { Logger::info(_("faq.a4")); Logger::press_enter(); return true; }));

        // 5: gvfs/udisks2 — 确认后卸载
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q5"), "5",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a5"));
                if (Logger::confirm(_("faq.confirm_exec")))
                    Executor::passthrough(
                        cfg_.remove_command + " --allow-change-held-packages ^udisks2 ^gvfs 2>/dev/null");
                Logger::press_enter(); return true;
            }));

        // 6: QQ crash — 确认后删配置
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q6"), "6",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a6"));
                if (Logger::confirm(_("faq.confirm_exec")))
                    Executor::passthrough("rm -rf ~/.config/tencent-qq/ 2>/dev/null");
                Logger::press_enter(); return true;
            }));

        // 7: VNC/X11 — 仅显示
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q7"), "7",
            [this](MenuContext&) -> bool { Logger::info(_("faq.a7")); Logger::press_enter(); return true; }));

        // 8: Baidu Netdisk — 确认后删 db
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q8"), "8",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a8"));
                if (Logger::confirm(_("faq.confirm_exec")))
                    Executor::passthrough("rm -vf ~/baidunetdisk/baidunetdiskdata.db 2>/dev/null");
                Logger::press_enter(); return true;
            }));

        // 9: mlocate — 确认后卸载
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q9"), "9",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a9"));
                if (Logger::confirm(_("faq.confirm_exec"))) {
                    domain::PackageManager::remove("mlocate",
                        domain::infer_family_from_config(cfg_.linux_distro));
                    domain::PackageManager::remove("catfish",
                        domain::infer_family_from_config(cfg_.linux_distro));
                    Executor::passthrough("sudo apt autoremove --purge 2>/dev/null");
                }
                Logger::press_enter(); return true;
            }));

        // 10: TTY Chinese — yesno 选择
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q10"), "10",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a10"));
                std::string tty_choice = cfg_.tui_bin +
                    " --title \"TTY Chinese\""
                    " --yes-button 'fbterm' --no-button 'export LANG'"
                    " --yesno \"" + _("faq.a10") + "\" 11 45";
                auto tty_rc = Executor::passthrough(tty_choice);
                if (tty_rc.exit_code == 0) {
                    if (!Executor::has("fbterm"))
                        domain::PackageManager::install("fbterm",
                            domain::infer_family_from_config(cfg_.linux_distro));
                    Executor::passthrough("fbterm 2>/dev/null");
                } else {
                    Logger::info("export LANG=C.UTF-8");
                }
                Logger::press_enter(); return true;
            }));

        // 11: time sync — 确认后完整同步
        menu->add_child(std::make_shared<LambdaAction>(_("faq.q11"), "11",
            [this](MenuContext&) -> bool {
                Logger::info(_("faq.a11"));
                if (!Logger::confirm(_("faq.a11_confirm"))) { Logger::press_enter(); return true; }
                Executor::passthrough("timedatectl set-local-rtc 1 --adjust-system-clock 2>/dev/null");
                domain::PackageManager::install({"ntpdate", "chrony"},
                    domain::infer_family_from_config(cfg_.linux_distro));
                Executor::passthrough("ntpdate time.windows.com 2>/dev/null");
                Executor::passthrough("timedatectl set-ntp true 2>/dev/null");
                Executor::passthrough(
                    "sudo systemctl enable chrony 2>/dev/null || "
                    "sudo sudo systemctl enable chronyd 2>/dev/null || "
                    "rc-update add chrony 2>/dev/null");
                Executor::passthrough("chronyc sourcestats -v 2>/dev/null");
                Logger::press_enter(); return true;
            }));

        add_sandwich_nav(menu);
        return menu;
    }

    std::shared_ptr<ui::IUIMenu> Manager::build_locale_menu() {
        using namespace tmoe::ui;

        auto menu = make_plugin_menu(_("locale.title"), _("locale.menu_prompt"), "locale");

        std::string cur = std::string(I18n::current_lang());
        menu->add_child(std::make_shared<LambdaAction>(
            _("locale.zh_cn") + (cur == "zh_CN" ? " ✓" : ""), "zh_CN",
            [this](MenuContext&) -> bool {
                I18n::init("zh_CN"); cfg_.locale = "zh_CN";
                environment_->set_locale("zh_CN");
                if (save_locale_) save_locale_("zh_CN");
                Logger::ok(_f("locale.switched_ok", "zh_CN"));
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(
            _("locale.en_us") + (cur == "en_US" ? " ✓" : ""), "en_US",
            [this](MenuContext&) -> bool {
                I18n::init("en_US"); cfg_.locale = "en_US";
                environment_->set_locale("en_US");
                if (save_locale_) save_locale_("en_US");
                Logger::ok(_f("locale.switched_ok", "en_US"));
                return true;
            }));

        add_sandwich_nav(menu);
        return menu;
    }

    std::shared_ptr<ui::IUIMenu> Manager::build_mirror_menu() {
        using namespace tmoe::ui;

        std::string extra_label = _("mirror.extra_unsupported");
        if (cfg_.linux_distro == "debian") extra_label = _("mirror.extra_debian_kali");
        else if (cfg_.linux_distro == "arch") extra_label = _("mirror.extra_archlinuxcn");
        else if (cfg_.linux_distro == "alpine") extra_label = _("mirror.extra_alpine");
        else if (cfg_.linux_distro == "redhat")
            extra_label = (cfg_.sub_distro == "fedora") ? _("mirror.extra_rpmfusion") : _("mirror.extra_epel");

        auto menu = make_plugin_menu(_("mirror.title"), _("mirror.menu_prompt"), "mirror_main");

        // 1-3: 按分类选镜像 → 进入动态镜像列表子菜单
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.cat_business"), "1",
            [this](MenuContext&) -> bool {
                select_mirror_from_category("business");
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.cat_university"), "2",
            [this](MenuContext&) -> bool {
                select_mirror_from_category("university");
                return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.cat_worldwide"), "3",
            [this](MenuContext&) -> bool {
                select_mirror_from_category("worldwide");
                return true;
            }));

        // 4-12: 单步操作
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.ping_test"), "4",
            [this](MenuContext&) -> bool {
                mirror_mgr_->run_ping_latency_test(); Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.speed_test"), "5",
            [this](MenuContext&) -> bool {
                mirror_mgr_->run_download_speed_test(); Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.restore_official"), "6",
            [this](MenuContext&) -> bool {
                if (Logger::confirm(_("mirror.confirm_restore_official")))
                    mirror_mgr_->restore_official();
                Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.edit_manual"), "7",
            [this](MenuContext&) -> bool {
                mirror_mgr_->edit_manually(); Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(extra_label, "8",
            [this](MenuContext&) -> bool {
                mirror_mgr_->manage_extra_sources(); Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.faq"), "9",
            [this](MenuContext&) -> bool {
                mirror_mgr_->show_mirror_faq(); Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.toggle_protocol"), "10",
            [this](MenuContext&) -> bool {
                std::string toggle_cmd = cfg_.tui_bin +
                    " --title \"" + _("mirror.protocol_title") + "\" --yes-button 'https' --no-button 'http'"
                    " --yesno \"" + _("mirror.select_protocol") + "\" 0 50";
                auto result = Executor::passthrough(toggle_cmd);
                mirror_mgr_->toggle_http_https(result.exit_code == 0);
                Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.clean_sources"), "11",
            [this](MenuContext&) -> bool {
                if (Logger::confirm(_("mirror.confirm_clean_sources")))
                    mirror_mgr_->clean_sources_list();
                Logger::press_enter(); return true;
            }));
        menu->add_child(std::make_shared<LambdaAction>(_("mirror.trust_sources"), "12",
            [this](MenuContext&) -> bool {
                std::string trust_cmd = cfg_.tui_bin +
                    " --title \"" + _("mirror.trust_title") + "\" --yes-button 'trust' --no-button 'untrust'"
                    " --yesno \"" + _("mirror.trust_warning") + "\" 0 50";
                auto result = Executor::passthrough(trust_cmd);
                mirror_mgr_->trust_sources(result.exit_code == 0);
                Logger::press_enter(); return true;
            }));

        add_sandwich_nav(menu);
        return menu;
    }

    /** 从指定分类选择镜像（被 build_mirror_menu 的 1-3 项调用）。 */
    void Manager::select_mirror_from_category(const std::string& category) {
        using namespace tmoe::ui;

        auto mirrors = domain::MirrorRegistry::instance().by_category(category);
        if (category == "worldwide") {
            auto extra = domain::MirrorRegistry::instance().by_region("global");
            mirrors.insert(mirrors.end(), extra.begin(), extra.end());
            for (const auto& r : {"hk", "tw", "jp", "kr", "us", "uk", "de", "fr", "ru", "au"}) {
                auto reg = domain::MirrorRegistry::instance().by_region(r);
                mirrors.insert(mirrors.end(), reg.begin(), reg.end());
            }
        }

        if (mirrors.empty()) {
            Logger::warn(_("mirror.no_available"));
            Logger::press_enter();
            return;
        }

        // 动态构建镜像选择子菜单
        auto menu = make_plugin_menu(
            _("mirror.select_mirror_title"), _("mirror.select_mirror_prompt"), "mirror_select");

        for (size_t i = 0; i < mirrors.size(); ++i) {
            auto label = mirrors[i].name + " (" + mirrors[i].url + ")";
            auto id = mirrors[i].id;
            menu->add_child(std::make_shared<ui::LambdaAction>(
                label, std::to_string(i + 1),
                [this, id](ui::MenuContext&) -> bool {
                    mirror_mgr_->switch_to(id);
                    Logger::press_enter();
                    return true;
                }));
        }

        add_sandwich_nav(menu);

        // 驱动子菜单
        ui::MenuContext ctx{cfg_};
        ui::MenuEngine engine(ctx);
        engine.run(menu);
    }

    int Manager::run_interactive_plugin() {
        using namespace tmoe::ui;

        MenuContext ctx{cfg_};

        // 直接使用 build_root_menu() 构建与旧 UI 一致的主菜单：
        //   Termux → 10 项容器管理
        //   Linux  → 9 项工具箱
        auto root = build_root_menu();

        MenuEngine engine(ctx);
        return engine.run(root);
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
                    Logger::info(_f("container.list_item_path", c.name(), c.rootfs_path()));
                }
                break;
            }
            case LaunchMode::ZshManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::ThemeManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::MirrorManager: {
                tmoe::ui::MenuContext ctx{cfg_};
                tmoe::ui::MenuEngine(ctx).run(build_mirror_menu());
                break;
            }

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
            case LaunchMode::ToolMenu:
                return run_interactive_plugin();

            case LaunchMode::Aria2Manager: {
                tmoe::ui::MenuContext ctx{cfg_};
                return tmoe::ui::MenuEngine(ctx).run(
                        ui::menus::create_download_menu(download_tools_.get())), 0;
            }

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

} // namespace tmoe::app
