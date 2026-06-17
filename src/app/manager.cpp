#include "manager.h"

namespace tmoe::app {
    Manager::Manager(TmoeConfig cfg) : cfg_(std::move(cfg)) {
        environment_ = std::make_unique<domain::Environment>(cfg_);
        container_mgr_ = std::make_unique<domain::ContainerManager>(cfg_);
        termux_ = std::make_unique<domain::TermuxManager>(cfg_);
        gui_ = std::make_unique<domain::GUIManager>(cfg_);
        mirror_mgr_ = std::make_unique<domain::MirrorManager>(cfg_);
        backup_mgr_ = std::make_unique<domain::BackupManager>(cfg_);
        docker_mgr_ = std::make_unique<domain::DockerManager>(cfg_);
        virt_mgr_ = std::make_unique<domain::VirtualizationManager>(cfg_);
        init_routes();
    }

    void Manager::init_routes() {
        // 1. Proot 容器向导
        tui_routes_["1"] = [this]() {
            termux_->check_and_init_environment();
            Logger::step("进入 Proot 容器向导...");
            auto container = container_mgr_->create("debian_default", "debian", "sid");

            if (!container_mgr_->find("debian_default").has_value()) {
                if (Logger::confirm("未检测到默认容器，是否立即安装 Debian Sid?")) {
                    container.install();
                }
            } else {
                container.start();
            }
        };

        // 2. Chroot (需要 root 权限)
        tui_routes_["2"] = [this]() {
            if (!cfg_.is_root) {
                Logger::error(_("error.no_root"));
                return;
            }
            Logger::step("唤起 Chroot 高级挂载模块...");
            // TODO: 实现 Chroot 容器启动逻辑
        };

        // 3. 移除/卸载容器
        tui_routes_["3"] = [this]() {
            Logger::step("进入移除/卸载管理模块...");
            auto containers = container_mgr_->list_all();
            if (containers.empty()) {
                Logger::warn("当前未安装任何容器！");
                return;
            }

            Logger::info("已安装的容器:");
            for (const auto &c: containers) {
                Logger::info(" - " + c.name() + " (" + c.distro() + ")");
            }
            // TODO: 扩展 whiptail 多选清单功能
            if (Logger::confirm("确认要卸载名为 default 的容器吗？此操作不可逆！")) {
                container_mgr_->remove("default");
                Logger::ok("已成功清理容器残留。");
            }
        };

        // 4. 语言区域重置
        tui_routes_["4"] = [this]() {
            Logger::step("正在重置终端 Locale 语言环境...");
            environment_->set_locale(cfg_.locale);
            Logger::ok("语言环境已重置为: " + cfg_.locale);
        };

        // 5. Termux 额外选项
        tui_routes_["5"] = [this]() {
            termux_->run_termux_menu();
        };

        // 6. 美化终端 (oh-my-zsh 等)
        tui_routes_["6"] = [this]() {
            termux_->beautify_terminal();
        };

        // 7. 通过 git pull 自更新
        tui_routes_["7"] = [this]() {
            Logger::step("正在拉取最新代码更新...");
            if (Executor::shell("git -C " + cfg_.work_dir.string() + " pull").ok()) {
                Logger::ok("更新成功！请重新启动 tmoes。");
            } else {
                Logger::error("更新失败，请检查网络或 Git 配置。");
            }
        };

        // 8. 常见问题
        tui_routes_["8"] = [this]() {
            Logger::info("=== FAQ ===");
            Logger::info("Q: Android 12+ 闪退怎么办？\nA: 请运行菜单 10 修复 signal 9 限制。");
            Logger::info("Q: 容器内无声音？\nA: 安装 pulseaudio 并检查环境变量。");
        };

        // 9. 反馈 Bug
        tui_routes_["9"] = [this]() {
            Logger::info("如需反馈 Bug，请访问 Github 仓库提交 Issue:");
            Logger::info("https://github.com/2-moe/tmoe-linux");
        };

        // 10. 修复 Android 12+ Signal 9
        tui_routes_["10"] = [this]() {
            termux_->fix_android_12_signal_9();
        };

        // 11. 镜像源管理子菜单
        tui_routes_["11"] = [this]() {
            run_mirror_menu();
        };

        // 12. GUI/VNC 远程桌面管理
        tui_routes_["12"] = [this]() {
            gui_->run_gui_menu();
        };

        // 13. 备份/恢复
        tui_routes_["13"] = [this]() {
            backup_mgr_->run_backup_menu();
        };

        // 14. Docker 容器管理
        tui_routes_["14"] = [this]() {
            docker_mgr_->run_docker_menu();
        };

        // 15. 虚拟化管理 (QEMU/VBox/Wine)
        tui_routes_["15"] = [this]() {
            virt_mgr_->run_virt_menu();
        };
    }

    /** 镜像源管理子菜单 (对应原始 Bash 脚本的 12 项 TUI)。 */
    void Manager::run_mirror_menu() {
        using namespace domain;

        while (true) {
            // 第一级：镜像源管理主菜单
            std::string menu_cmd = cfg_.tui_bin +
                                   " --title \"镜像源管理 — " + cfg_.os_pretty_name + "\"" +
                                   " --menu \"请选择操作 (当前发行版: " + cfg_.linux_distro + ")\" 0 0 0 "
                                   "\"1\" \"商业镜像源 (华为云/阿里云/腾讯云/网易…)\" "
                                   "\"2\" \"高校镜像源 (清华/中科大/上交/浙大…)\" "
                                   "\"3\" \"全球镜像站 (Debian/Arch 官方源)\" "
                                   "\"4\" \"自动选择最优镜像 (ping 测速)\" "
                                   "\"5\" \"还原为默认官方源\" "
                                   "\"6\" \"手动编辑源列表\" "
                                   "\"7\" \"HTTP ⇄ HTTPS 协议切换\" "
                                   "\"8\" \"清理无效行 (去注释+去重)\" "
                                   "\"9\" \"强制信任软件源\" "
                                   "\"0\" \"返回上级菜单\"";

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
                    Logger::warn("该分类下无可用镜像站");
                    Logger::press_enter();
                    continue;
                }

                // 构建 whiptail menu 字符串
                std::string sub_menu = cfg_.tui_bin + " --title \"选择镜像站\" --menu \"请选择镜像站:\" 0 0 0 ";
                for (size_t i = 0; i < mirrors.size(); ++i) {
                    sub_menu += "\"" + std::to_string(i + 1) + "\" \""
                            + mirrors[i].name + " (" + mirrors[i].url + ")\" ";
                }
                sub_menu += "\"0\" \"返回\"";

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
                if (Logger::confirm("确认还原为默认官方源？这将覆盖当前源配置。")) {
                    mirror_mgr_->restore_official();
                }
                Logger::press_enter();
            } else if (choice == "6") {
                mirror_mgr_->edit_manually();
                Logger::press_enter();
            } else if (choice == "7") {
                std::string toggle_cmd = cfg_.tui_bin +
                                         " --title \"HTTP / HTTPS\" --yes-button \"切换为 HTTPS\" --no-button \"切换为 HTTP\""
                                         " --yesno \"请选择协议:\" 0 0";
                auto result = Executor::shell(toggle_cmd);
                bool use_https = result.exit_code == 0;
                mirror_mgr_->toggle_http_https(use_https);
                Logger::press_enter();
            } else if (choice == "8") {
                if (Logger::confirm("将删除源列表中的所有注释行并去除重复行，确认？")) {
                    mirror_mgr_->clean_sources_list();
                }
                Logger::press_enter();
            } else if (choice == "9") {
                std::string trust_cmd = cfg_.tui_bin +
                                        " --title \"强制信任\" --yes-button \"信任\" --no-button \"取消信任\""
                                        " --yesno \"强制信任软件源？\\n(可能有安全风险)\" 0 0";
                auto result = Executor::shell(trust_cmd);
                bool trust = result.exit_code == 0;
                mirror_mgr_->trust_sources(trust);
                Logger::press_enter();
            }
        }
    }

    std::string Manager::render_and_get_choice() {
        std::string cur_lang = std::string(I18n::current_lang());
        std::string title = "tmoes manager running on " + cfg_.os_pretty_name;

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
                "\"12\" \"" + _("menu.tui.gui") + "\" "
                "\"13\" \"" + _("menu.tui.backup") + "\" "
                "\"14\" \"" + _("menu.tui.docker") + "\" "
                "\"15\" \"" + _("menu.tui.virt") + "\" "
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
                Logger::press_enter();
            } else {
                Logger::warn("即将到来的新功能交互项: " + choice);
                Logger::press_enter();
            }
        }

        Logger::info(_("goodbye"));
        return 0;
    }

    /** 直接从命令行参数执行领域逻辑。 */
    int Manager::run_launch_context(const LaunchContext &ctx) {
        if (ctx.mode == LaunchMode::Help) {
            // TODO: 打印详细帮助
            return 0;
        }

        environment_->initialize();

        switch (ctx.mode) {
            case LaunchMode::Proot: {
                Logger::info("核心容器调度: [模式: PRoot] -> [发行版: " + ctx.distro_name + "]");

                std::string container_name = ctx.distro_name.empty() ? "default" : ctx.distro_name;
                auto container = container_mgr_->create(container_name, ctx.distro_name, ctx.distro_code,
                                                        domain::ContainerMode::Proot);

                // 启动前钩子
                bool is_gui_mode = (ctx.exec_program == "startvnc" || ctx.exec_program == "novnc" || ctx.exec_program_01
                                    == "startvnc");

                if (is_gui_mode) {
                    gui_->start_pulseaudio_bridge();
                }

                if (!container.start(&ctx)) {
                    Logger::error("容器运行异常终止");
                    return 1;
                }

                // 启动后钩子
                if (is_gui_mode) {
                    // 给 VNC/noVNC 服务一点初始化时间
                    Executor::shell("sleep 3");

                    std::string uri = (ctx.exec_program == "novnc" || ctx.exec_program_01 == "novnc")
                                          ? "http://127.0.0.1:6080"
                                          : "vnc://127.0.0.1:5901";

                    environment_->open_uri(uri);
                }

                break;
            }
            case LaunchMode::ListContainers: {
                auto list = container_mgr_->list_all();
                Logger::info("已安装的容器列表:");
                for (const auto &c: list) {
                    std::fprintf(stderr, "  - %s [%s]\n", c.name().c_str(), c.rootfs_path().c_str());
                }
                break;
            }
            case LaunchMode::Chroot:
                // TODO: 实现 Chroot 启动
                Logger::warn("Chroot 模式暂未实现");
                break;
            case LaunchMode::Nspawn:
                // TODO: 实现 systemd-nspawn 启动
                Logger::warn("systemd-nspawn 模式暂未实现");
                break;
            case LaunchMode::ZshManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::MirrorManager:
                run_mirror_menu();
                break;
            case LaunchMode::DebianInstall:
                // TODO: 实现一键安装 Debian
                Logger::warn("一键安装 Debian 暂未实现");
                break;
            case LaunchMode::ManagerMenu:
                // TODO: 实现管理菜单跳转
                break;
            case LaunchMode::ToolMenu:
                // TODO: 实现工具菜单跳转
                break;
            case LaunchMode::Aria2Manager:
                // TODO: 实现 Aria2 管理器
                break;
            default:
                Logger::warn("路由模式未完全实现或未定义");
                break;
        }
        return 0;
    }
} // namespace tmoe::app
