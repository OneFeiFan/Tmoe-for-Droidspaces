#include "app/manager.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/executor.h"

namespace tmoe::app {
    Manager::Manager(TmoeConfig cfg) : cfg_(std::move(cfg)) {
        environment_ = std::make_unique<domain::Environment>(cfg_);
        container_mgr_ = std::make_unique<domain::ContainerManager>(cfg_);
        termux_ = std::make_unique<domain::TermuxManager>(cfg_);
        // ── 新增：实例化 GUIManager ──
        gui_ = std::make_unique<domain::GUIManager>(cfg_);
        // ── 新增：实例化 MirrorManager ──
        mirror_mgr_ = std::make_unique<domain::MirrorManager>(cfg_);
        // 构造时初始化路由表
        init_routes();
    }

    void Manager::init_routes() {
        tui_routes_["1"] = [this]() {
            termux_->check_and_init_environment();
            Logger::step("进入 Proot 容器向导...");
            // 利用工厂创建一个新容器
            auto container = container_mgr_->create("debian_default", "debian", "sid");

            // 如果不存在则安装
            if (!container_mgr_->find("debian_default").has_value()) {
                if (Logger::confirm("未检测到默认容器，是否立即安装 Debian Sid?")) {
                    container.install();
                }
            } else {
                container.start();
            }
        };

        tui_routes_["2"] = [this]() {
            if (!cfg_.is_root) {
                Logger::error(_("error.no_root"));
                return;
            }
            Logger::step("唤起 Chroot 高级挂载模块...");
            // TODO: Chroot 逻辑
        };

        // 3. 移除/卸载管理
        tui_routes_["3"] = [this]() {
            Logger::step("进入移除/卸载管理模块...");
            auto containers = container_mgr_->list_all();
            if (containers.empty()) {
                Logger::warn("当前未安装任何容器！");
                return;
            }

            // 简单列出并请求输入，实际可进一步拓展出 whiptail checklist
            Logger::info("已安装的容器:");
            for (const auto &c: containers) {
                Logger::info(" - " + c.name() + " (" + c.distro() + ")");
            }
            if (Logger::confirm("确认要卸载名为 default 的容器吗？此操作不可逆！")) {
                container_mgr_->remove("default");
                Logger::ok("已成功清理容器残留。");
            }
        };

        // 4. 语言/区域重置
        tui_routes_["4"] = [this]() {
            Logger::step("正在重置终端 Locale 语言环境...");
            environment_->set_locale(cfg_.locale);
            Logger::ok("语言环境已重置为: " + cfg_.locale);
        };

        tui_routes_["5"] = [this]() {
            Logger::step("进入 Android-Termux 额外选项面板...");
        };

        tui_routes_["6"] = [this]() {
            termux_->beautify_terminal();
        };

        // 7. 更新 Tmoe
        tui_routes_["7"] = [this]() {
            Logger::step("正在拉取最新代码更新...");
            // 实用手段：直接调 git pull
            if (Executor::shell("git -C " + cfg_.work_dir.string() + " pull").ok()) {
                Logger::ok("更新成功！请重新启动 tmoes。");
            } else {
                Logger::error("更新失败，请检查网络或 Git 配置。");
            }
        };

        // 8. FAQ
        tui_routes_["8"] = [this]() {
            Logger::info("=== FAQ ===");
            Logger::info("Q: Android 12+ 闪退怎么办？\nA: 请运行菜单 10 修复 signal 9 限制。");
            Logger::info("Q: 容器内无声音？\nA: 安装 pulseaudio 并检查环境变量。");
        };

        // 9. 反馈
        tui_routes_["9"] = [this]() {
            Logger::info("如需反馈 Bug，请访问 Github 仓库提交 Issue:");
            Logger::info("https://github.com/2-moe/tmoe-linux");
        };

        tui_routes_["10"] = [this]() {
            termux_->fix_android_12_signal_9();
        };

        tui_routes_["11"] = [this]() {
            run_mirror_menu();
        };

        // 后续新增菜单项，只需在此处追加 tui_routes_["x"] = ... 即可，完全解耦！
    }

    // ──────────────────────────────────────────────────
    //  镜像源子菜单 (对应 Bash gnu-linux_mirrors 的 12 项 TUI)
    // ──────────────────────────────────────────────────
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
                    for (const auto& r : {"hk", "tw", "jp", "kr", "us", "uk", "de", "fr", "ru", "au"}) {
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
                "\"3\" \"" + _("menu.tui.remove") + "\" " // 补回：卸载
                "\"4\" \"" + _("menu.tui.locale") + "\" " // 补回：语言区域
                "\"5\" \"" + _("menu.tui.termux") + "\" "
                "\"6\" \"" + _("menu.tui.zsh") + "\" "
                "\"7\" \"" + _("menu.tui.update") + "\" " // 补回：更新
                "\"8\" \"" + _("menu.tui.faq") + "\" " // 补回：常见问题
                "\"9\" \"" + _("menu.tui.report") + "\" " // 补回：反馈bug
                "\"10\" \"" + _("menu.tui.fix_signal9") + "\" "
                "\"11\" \"" + _("menu.tui.mirrors") + "\" "
                "\"0\" \"" + _("menu.tui.exit") + "\"";

        return Executor::tui_select(menu_cmd);
    }

    int Manager::run_interactive() {
        Logger::info(_("welcome"));
        environment_->initialize();

        // 1. 触发宿主机的全自动依赖包安装 (此时已是 Root，安全执行)
        environment_->check_dependencies();

        // 2. 触发 Android 的 TUI 补丁，并动态修正 Manager 实例中的 tui_bin 路径
        if (cfg_.is_termux) {
            cfg_.tui_bin = termux_->check_and_patch_tui_env();
        }

        // 3. 原有的环境前置检查 (配色/字体/崩溃修复等)
        termux_->check_and_init_environment();

        while (true) {
            // 这里 render_and_get_choice() 就会使用刚刚动态修正好的 cfg_.tui_bin
            std::string choice = render_and_get_choice();

            if (choice == "0" || choice.empty()) {
                break;
            }

            // 核心调度：通过路由表分发执行
            auto it = tui_routes_.find(choice);
            if (it != tui_routes_.end()) {
                it->second(); // 执行绑定的 Lambda
                Logger::press_enter();
            } else {
                Logger::warn("即将到来的新功能交互项: " + choice);
                Logger::press_enter();
            }
        }

        Logger::info(_("goodbye"));
        return 0;
    }

    // ── 闭环打通：命令行参数直接触发底层引擎 ──
    int Manager::run_launch_context(const LaunchContext &ctx) {
        if (ctx.mode == LaunchMode::Help) {
            // 打印帮助略...
            return 0;
        }

        environment_->initialize();

        switch (ctx.mode) {
            case LaunchMode::Proot: {
                Logger::info("核心容器调度: [模式: PRoot] -> [发行版: " + ctx.distro_name + "]");

                std::string container_name = ctx.distro_name.empty() ? "default" : ctx.distro_name;
                auto container = container_mgr_->create(container_name, ctx.distro_name, ctx.distro_code,
                                                        domain::ContainerMode::Proot);

                // ── 新增：预触发钩子 (Pre-Launch Hooks) ──
                bool is_gui_mode = (ctx.exec_program == "startvnc" || ctx.exec_program == "novnc" || ctx.exec_program_01 == "startvnc");

                if (is_gui_mode) {
                    gui_->start_pulseaudio_bridge();
                }

                // 核心修复：将带有脚本投递和执行指令的上下文 ctx 传入
                if (!container.start(&ctx)) {
                    Logger::error("容器运行异常终止");
                    return 1;
                }

                // ── 新增：后触发钩子 (Post-Launch Hooks) ──
                if (is_gui_mode) {
                    // 容器在后台启动了 VNC/noVNC，我们稍微等待 3 秒让服务就绪
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
            case LaunchMode::ZshManager:
                termux_->beautify_terminal();
                break;
            case LaunchMode::MirrorManager:
                run_mirror_menu();
                break;
            // ... 其他状态机匹配
            default:
                Logger::warn("路由模式未完全实现或未定义");
                break;
        }
        return 0;
    }
} // namespace tmoe::app
