#include "desktop_manager.h"
#include "domain/desktops/desktop_factory.h"
#include "domain/desktops/desktop_utils.h"
#include "core/system_helper.h"
#include <sstream>

namespace tmoe::domain {
    DesktopManager::DesktopManager(const TmoeConfig &cfg, VncManager &vnc_manager)
        : cfg_(cfg), vnc_manager_(vnc_manager) {
    }

    DistroFamily DesktopManager::resolved_family() const {
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        return family;
    }

    bool DesktopManager::install_packages(const std::vector<std::string> &pkgs) const {
        return PackageManager::install(pkgs, resolved_family());
    }

    void DesktopManager::set_auto_install_flags(bool fcitx, bool electron, bool vscode,
                                                bool chromium, bool kali,
                                                const std::string &kali_tools) {
        auto_install_fcitx4_ = fcitx;
        auto_install_electron_ = electron;
        auto_install_vscode_ = vscode;
        auto_install_chromium_ = chromium;
        auto_install_kali_ = kali;
        kali_tools_ = kali_tools;
    }

    // ═══════════════════════════════════════════════════════════════
    // 桌面环境注册表
    // ═══════════════════════════════════════════════════════════════

    const std::vector<DesktopInfo> &DesktopManager::desktop_registry() const {
        return gui_config::all_desktops();
    }

    DesktopInfo DesktopManager::get_desktop_info(std::string_view desktop) const {
        for (const auto &info: desktop_registry())
            if (info.id == desktop) return info;
        return desktop_registry().front();
    }

    void DesktopManager::preconfigure_gui_dependencies() {
        Logger::step(_("gui.preconfig_deps"));
        auto family = resolved_family();
        auto deps = gui_config::distro_gui_deps(PackageManager::family_key(family));

        // Gentoo 特殊: 先运行 dispatch-conf etc-update
        if (family == DistroFamily::Gentoo) {
            Executor::shell("dispatch-conf 2>/dev/null || true");
            Executor::shell("etc-update 2>/dev/null || true");
        }

        // 辅助: 将空格分隔的包名转为 vector
        auto split_pkgs = [](const std::string &s) -> std::vector<std::string> {
            std::vector<std::string> out;
            std::istringstream iss(s);
            std::string token;
            while (iss >> token) out.push_back(token);
            return out;
        };

        // 安装核心包 (使用 PackageManager 以获得 eatmydata/回退等完整逻辑)
        if (!deps.extra_pkgs.empty())
            PackageManager::install(split_pkgs(deps.extra_pkgs), family);
        if (!deps.vnc_pkg.empty() && !deps.skip_vnc)
            PackageManager::install(split_pkgs(deps.vnc_pkg), family);
        if (!deps.font_pkg.empty())
            PackageManager::install(split_pkgs(deps.font_pkg), family);

        Logger::ok(_("gui.preconfig_deps_done"));
    }


    bool DesktopManager::install_desktop(std::string_view desktop) {
        auto desktop_obj = DesktopFactory::create(desktop, cfg_);
        if (!desktop_obj) {
            Logger::error("Unknown desktop: " + std::string(desktop));
            return false;
        }
        const auto &info = desktop_obj->get_info();
        Logger::step(_f("gui.install.desktop", info.name));
        if (info.requires_root && cfg_.is_termux) Logger::warn(_f("gui.desktop.systemd_warn", info.name));
        if (!auto_install_mode_ && !info.is_window_manager) post_desktop_install_prompts();

        preconfigure_gui_dependencies();
        desktop_obj->will_be_installed_message();

        auto family = resolved_family();
        auto choices = desktop_obj->pre_install_choices(family, auto_install_mode_);
        std::string pkg_list = choices.pkg_list.empty()
                                   ? desktop_utils::resolve_distro_pkg_list(info, family)
                                   : choices.pkg_list;

        std::vector<std::string> pkgs;
        std::istringstream iss(pkg_list);
        std::string pkg;
        while (iss >> pkg) pkgs.emplace_back(pkg);
        pkgs.emplace_back("dbus-x11");

        bool pkg_ok = choices.use_no_recommends
                          ? Executor::passthrough(
                              "sudo apt install -y --no-install-recommends " + pkg_list + " 2>/dev/null").ok()
                          : install_packages(pkgs);
        if (!pkg_ok) {
            Logger::error(_f("gui.install.fail", info.name));
            return false;
        }

        PostInstallContext ctx{
            family, family == DistroFamily::Debian,
            cfg_.sub_distro == "ubuntu", cfg_.is_termux || cfg_.linux_distro == "Android"
        };
        Logger::step("Post-install config...");
        desktop_obj->post_install_config(ctx);
        Logger::step("Desktop extras...");
        desktop_obj->post_install_extras(ctx);

        if (!vnc_manager_.configure_xstartup(desktop)) Logger::warn(_("gui.desktop.xstartup_warn"));
        Logger::ok(_f("gui.install.success", info.name));
        if (!info.compat_notes.empty()) Logger::info(_f("gui.desktop.compat_notes", info.compat_notes));
        if (desktop_obj->recommends_tiger_vnc()) {
            vnc_manager_.config().server = "tiger";
            vnc_manager_.config().server_bin = "tigervnc";
        }
        execute_optional_installs();
        return true;
    }

    void DesktopManager::select_kali_tools() {
        // 对应旧 Bash do_you_want_to_install_kali_tools
        // Kali Linux 工具包选择子菜单
        std::string menu_cmd = cfg_.tui_bin +
                               " --title \"KALI LINUX TOOLS\""
                               " --menu \"请选择要安装的 Kali Linux 工具包\\n"
                               "Select Kali Linux tools metapackage\" 0 0 0 "
                               "\"arm\" \"kali-linux-arm (ARM设备工具)\" "
                               "\"None\" \"不安装工具包 (Skip)\" "
                               "\"core\" \"kali-linux-core (核心工具)\" "
                               "\"default\" \"kali-linux-default (默认工具集)\" "
                               "\"everything\" \"kali-linux-everything (全部工具)\" "
                               "\"headless\" \"kali-linux-headless (无头模式)\" "
                               "\"large\" \"kali-linux-large (大型工具集)\" "
                               "\"nethunter\" \"kali-linux-nethunter (NetHunter)\" "
                               "\"0\" \"" + std::string(_("menu.tui.back")) + "\"";

        std::string choice = Executor::tui_select(menu_cmd);
        if (choice == "0" || choice.empty() || choice == "None") return;

        // 映射选项到包名
        std::string pkg_name;
        if (choice == "arm") pkg_name = "kali-linux-arm";
        else if (choice == "core") pkg_name = "kali-linux-core";
        else if (choice == "default") pkg_name = "kali-linux-default";
        else if (choice == "everything") pkg_name = "kali-linux-everything";
        else if (choice == "headless") pkg_name = "kali-linux-headless";
        else if (choice == "large") pkg_name = "kali-linux-large";
        else if (choice == "nethunter") pkg_name = "kali-linux-nethunter";

        if (!pkg_name.empty()) {
            install_packages({pkg_name});
        }
    }


    void DesktopManager::post_desktop_install_prompts() {
        // 对应旧 Bash auto_install_and_configure_fcitx4 + do_you_want_to_install_fcitx4 追问链
        bool interactive = !auto_install_mode_;

        auto family = resolved_family();

        auto lang = std::string(I18n::current_lang());
        bool want_fcitx = false;
        bool want_chromium = auto_install_chromium_;
        bool want_electron = auto_install_electron_;
        bool want_kali = auto_install_kali_;

        // 1. fcitx: 中文环境, debian/arch
        // Bash: WSL 用 fcitx_pinyin，其他用 fcitx4
        if (family == DistroFamily::Debian || family == DistroFamily::Arch) {
            bool is_zh = (lang.find("zh") == 0);
            if ((is_zh || cfg_.is_wsl) &&
                !Executor::has("fcitx") && !Executor::has("fcitx5")) {
                if (interactive) {
                    auto r = Executor::passthrough(cfg_.tui_bin +
                                                   " --title \"input method\""
                                                   " --defaultno"
                                                   " --yesno '" + std::string(cfg_.is_wsl
                                                                                  ? "检测到WSL环境,是否需要安装中文输入法(fcitx-pinyin)？"
                                                                                  : "检测到您当前的语言环境为中文，是否需要安装中文输入法？") +
                                                   "\\n"
                                                   "安装完成后，在桌面环境下按Ctrl+空格切换输入法\\n"
                                                   "你也可以选择NO跳过，之后可以单独安装fcitx5' 0 0");
                    want_fcitx = (r.exit_code == 0);
                } else {
                    want_fcitx = auto_install_fcitx4_;
                }
            }
        }

        // 2. Chromium（bash: 已安装则跳过提示）
        if (!Executor::has("chromium") && !Executor::has("chromium-browser")
            && !Executor::has("google-chrome") && !Executor::has("google-chrome-stable")) {
            if (interactive) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"CHROMIUM-BROWSER\""
                                               " --defaultno"
                                               " --yesno 'Do you want to install Google Chromium browser?' 0 0");
                want_chromium = (r.exit_code == 0);
            }
        }

        // 3. Electron 应用合集（bash: alpine 排除，已安装则跳过）
        if (family != DistroFamily::Alpine && !Executor::has("electron")) {
            if (interactive) {
                auto r = Executor::passthrough(cfg_.tui_bin +
                                               " --title \"Electron apps\""
                                               " --defaultno"
                                               " --yesno '请问您是否需要安装开发者推荐的electron软件包合集？\\n"
                                               "该合集包含哔哩哔哩客户端，obsidian(markdown编辑器)，\\n"
                                               "网易云音乐第三方electron版，listen1，\\n"
                                               "YesPlayMusic，petal和zy-player\\n"
                                               "你可以选择NO跳过，之后可以单独安装electron app。' 0 0");
                want_electron = (r.exit_code == 0);
            }
        }

        // 4. Kali tools
        bool is_kali = (cfg_.linux_distro == "kali" || cfg_.linux_distro.find("kali") != std::string::npos);
        if (is_kali && interactive) {
            select_kali_tools();
        }

        // ── 保存选择（后续由 execute_optional_installs() 统一安装） ──
        auto_install_fcitx4_ = want_fcitx;
        auto_install_chromium_ = want_chromium;
        auto_install_electron_ = want_electron;
    }


    void DesktopManager::execute_optional_installs() {
        // 对齐 Bash auto_install_and_configure_fcitx4()
        // 执行 post_desktop_install_prompts 中用户确认的可选安装

        bool interactive = !auto_install_mode_;

        if (auto_install_fcitx4_) {
            install_fcitx();
        }

        if (auto_install_chromium_) {
            auto family = resolved_family();
            std::vector<std::string> chromium_pkgs;
            if (family == DistroFamily::Debian) {
                chromium_pkgs = {"chromium-browser", "chromium"};
            } else if (family == DistroFamily::Arch) {
                chromium_pkgs = {"chromium"};
            } else {
                chromium_pkgs = {"chromium-browser", "chromium", "google-chrome-stable"};
            }
            install_packages(chromium_pkgs);
        }

        if (auto_install_electron_) {
            install_packages({
                "bilibili", "electron-netease-cloud-music",
                "obsidian", "listen1", "yesplaymusic", "petal", "zy-player"
            });
        }

        if (auto_install_vscode_ && !interactive) {
            Executor::passthrough(cfg_.install_command + " code 2>/dev/null || "
                                  "sudo snap install code --classic 2>/dev/null || true");
        }

        // Ubuntu language support 已在 get_ubuntu_desktop_language_pack() 中处理
        // (language-pack-zh-hans, language-pack-gnome-zh-hans 直接装，不走 check-language-support)
    }


    bool DesktopManager::install_window_manager(std::string_view wm) {
        // 统一走 install_desktop (DesktopInfo 已包含所有 WM 条目)
        return install_desktop(wm);
    }


    void DesktopManager::after_desktop_install_hint() const {
        Logger::info(_("gui.after_install.hint"));
    }


    void DesktopManager::download_iosevka_ttf_font_ext() {
        // 对应旧 Bash download_iosevka_ttf_font (gui:388-444)
        const char *iosevka_file = "/usr/share/fonts/truetype/iosevka/Iosevka-Term-Mono.ttf";
        if (fs::exists(iosevka_file)) return;

        Logger::info(_("gui.installing_iosevka"));
        CommandBuilder("sudo").add_arg("mkdir").add_flag("-pv").add_arg("/usr/share/fonts/truetype/iosevka/").add_raw("2>/dev/null").
                execute();

        // 检查 /tmp/font.ttf 是否已存在且 sha256 匹配
        auto sha_check = Executor::shell("cd /tmp && [ -e font.ttf ] && sha256sum font.ttf 2>/dev/null");
        std::string expected_sha = "cb4f09f9ec1b0d21021dce6c6dbe4f7ecb4930cbea0c766da1fe478111a5844e";
        if (sha_check.ok() && sha_check.stdout_data.find(expected_sha) != std::string::npos) {
            CommandBuilder("sudo").add_arg("cp").add_flag("-fv").add_arg("/tmp/font.ttf").add_arg(iosevka_file).execute();
            return;
        } else if (fs::exists("/tmp/font.ttf")) {
            Executor::shell("sudo mv -vf /tmp/font.ttf /usr/share/fonts/truetype/iosevka/Iosevka.ttf 2>/dev/null || true");
        }

        // 确定字体缓存目录
        std::string font_dir;
        if (fs::exists("/etc/gitstatus")) {
            if (fs::exists("/root/.cache/gitstatus")) {
                Executor::shell("sudo cp -f /root/.cache/gitstatus/* /etc/gitstatus 2>/dev/null || true");
                CommandBuilder("sudo").add_arg("chmod").add_flag("-R").add_arg("a+rx").add_arg("/etc/gitstatus/").add_raw(
                    "2>/dev/null || true").execute();
            }
            font_dir = "/etc/gitstatus";
        } else {
            font_dir = "/root/.cache/gitstatus";
            CommandBuilder("mkdir").add_flag("-pv").add_arg(font_dir).add_raw("2>/dev/null").execute();
        }

        // 从缓存解压
        if (fs::exists(font_dir + "/Iosevka-Term-Mono.tar.xz")) {
            Executor::shell("cd " + font_dir + " && tar -Jxvf Iosevka-Term-Mono.tar.xz 2>/dev/null && "
                            "sudo mv -vf Iosevka.ttf " + std::string(iosevka_file) + " 2>/dev/null || true");
        }

        // 下载字体
        if (!fs::exists(iosevka_file)) {
            std::string font_url_01 = "https://gitee.com/ak2/inconsolata-go-font/raw/master/Iosevka-Term-Mono.tar.xz";
            std::string font_url_02 =
                    "https://github.com/cu233/Iosevka-Term-Mono-Font/raw/master/Iosevka-Term-Mono.tar.xz";
            Executor::shell("cd " + font_dir + " && "
                            "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -o Iosevka-Term-Mono.tar.xz "
                            + font_url_02 + " 2>/dev/null || "
                            "curl -Lo 'Iosevka-Term-Mono.tar.xz' " + font_url_01 + " 2>/dev/null) && "
                            "tar -Jxvf 'Iosevka-Term-Mono.tar.xz' 2>/dev/null && "
                            "sudo mv -vf Iosevka.ttf " + std::string(iosevka_file) + " 2>/dev/null || true");
        }

        // 二次检查
        if (!fs::exists(iosevka_file)) {
            Executor::shell("sudo rm -fv " + font_dir + "/Iosevka-Term-Mono.tar.xz 2>/dev/null || true");
        }

        // 刷新字体缓存
        Executor::shell("cd /usr/share/fonts/truetype/iosevka/ && "
            "sudo mkfontscale 2>/dev/null; sudo mkfontdir 2>/dev/null; sudo fc-cache 2>/dev/null || true");
    }


    bool DesktopManager::install_fcitx() {
        Logger::step(_("gui.fcitx.install"));

        // fcitx + 拼音 + 配置工具
        std::vector<std::string> pkgs = {
            "fcitx", "fcitx-pinyin", "fcitx-config-gtk", "fcitx-frontend-gtk2",
            "fcitx-frontend-gtk3", "fcitx-frontend-qt5"
        };

        if (!install_packages(pkgs)) {
            Logger::warn(_("gui.fcitx.partial_fail"));
        }

        // 配置环境变量
        std::string home = SystemHelper::user_home();
        std::string bashrc = home + "/.bashrc";
        std::string env_block =
                "\n# tmoe fcitx (已自动配置)\n"
                "export GTK_IM_MODULE=fcitx\n"
                "export QT_IM_MODULE=fcitx\n"
                "export XMODIFIERS=@im=fcitx\n"
                "export DefaultIMModule=fcitx\n";
        SystemHelper::append_file(fs::path(bashrc), env_block);

        // 创建 fcitx 自启动
        std::string autostart_dir = home + "/.config/autostart";
        CommandBuilder("mkdir").add_flag("-p").add_arg(autostart_dir).execute();
        std::string fcitx_desktop = autostart_dir + "/fcitx.desktop";
        std::string desktop_content =
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Fcitx\n"
                "Exec=fcitx-autostart\n"
                "X-GNOME-Autostart-enabled=true\n";
        SystemHelper::write_file(fs::path(fcitx_desktop), desktop_content);

        Logger::ok(_("gui.fcitx.install_ok"));
        return true;
    }

    void DesktopManager::install_kali_undercover() {
        if (fs::exists("/usr/share/icons/Windows-10-Icons")) {
            Logger::info(_("gui.kali.undercover_exists"));
            return;
        }
        Logger::step(_("gui.kali.undercover_install"));
        Executor::shell("mkdir -pv /tmp/.kali-undercover-win10-theme && cd /tmp/.kali-undercover-win10-theme");
        std::string repo = "https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-undercover";
        Executor::shell("cd /tmp/.kali-undercover-win10-theme && "
                        "UNDERCOVERlatestLINK=\"$(curl -L '" + repo + "/' 2>/dev/null | grep all.deb | tail -n 1 | "
                        "cut -d '=' -f 3 | cut -d '\"' -f 2)\" && "
                        "(aria2c --console-log-level=warn --no-conf --allow-overwrite=true -s 5 -x 5 -k 1M "
                        "-o kali-undercover.deb '" + repo + "/'\"$UNDERCOVERlatestLINK\" 2>/dev/null || "
                        "apt download kali-undercover 2>/dev/null && mv *deb kali-undercover.deb) && "
                        "ar xv kali-undercover.deb && cd / && "
                        "sudo tar -Jxvf /tmp/.kali-undercover-win10-theme/data.tar.xz ./usr 2>/dev/null && "
                        "sudo mv -f /usr/bin/kali-undercover /usr/local/bin/ 2>/dev/null; "
                        "sudo update-icon-caches /usr/share/icons/Windows-10-Icons 2>/dev/null &");
        Executor::shell("rm -rfv /tmp/.kali-undercover-win10-theme 2>/dev/null || true");
        Logger::ok(_("gui.kali.undercover_ok"));
    }


    void DesktopManager::download_macos_mojave_theme() {
        Logger::step(_("gui.macos_mojave.install"));
        if (fs::exists("/usr/share/themes/Mojave-dark")) {
            Logger::info(_("gui.theme.already_installed"));
            return;
        }
        Executor::shell("cd /tmp && [ -d McMojave ] && rm -rf McMojave; "
            "git clone -b McMojave --depth=1 https://gitee.com/mo2/xfce-themes.git /tmp/McMojave 2>/dev/null && "
            "cd /tmp/McMojave && "
            "sudo tar -Jxvf 01-Mojave-dark.tar.xz -C /usr/share/themes 2>/dev/null && "
            "sudo tar -Jxvf 01-McMojave-circle.tar.xz -C /usr/share/icons 2>/dev/null && "
            "sudo update-icon-caches /usr/share/icons/McMojave-circle-dark /usr/share/icons/McMojave-circle 2>/dev/null &");
        Executor::shell("rm -rf /tmp/McMojave 2>/dev/null || true");
        set_default_xfce_icon_theme("McMojave-circle");
        Logger::ok(_("gui.macos_mojave.ok"));
    }


    void DesktopManager::download_macos_bigsur_theme() {
        Logger::step(_("gui.macos_bigsur.install"));
        if (fs::exists("/usr/share/icons/WhiteSur-dark")) {
            Logger::info(_("gui.theme.already_installed"));
            return;
        }
        Executor::shell("cd /tmp && [ -d BIGSUR_TEMP_FOLDER ] && rm -rvf BIGSUR_TEMP_FOLDER; "
            "git clone -b master --depth=1 https://gitee.com/ak2/bigsur-gtk-theme.git "
            "/tmp/BIGSUR_TEMP_FOLDER 2>/dev/null && "
            "cd /tmp/BIGSUR_TEMP_FOLDER && "
            "sudo tar -Jxvf WhiteSur.tar.xz -C /usr/share/icons 2>/dev/null && "
            "sudo tar -Jxvf WhiteSur-light-alt.tar.xz -C /usr/share/themes 2>/dev/null && "
            "sudo tar -Jxvf WhiteSur-dark.tar.xz -C /usr/share/themes 2>/dev/null && "
            "sudo update-icon-caches /usr/share/icons/WhiteSur /usr/share/icons/WhiteSur-dark 2>/dev/null &");
        Executor::shell("rm -rvf /tmp/BIGSUR_TEMP_FOLDER 2>/dev/null || true");
        set_default_xfce_icon_theme("WhiteSur");
        Logger::ok(_("gui.macos_bigsur.ok"));
    }


    void DesktopManager::download_arch_breeze_adapta_cursor_theme() {
        if (fs::exists("/usr/share/icons/Breeze-Adapta-Cursor")) return;
        Logger::step(_("gui.breeze.cursor_download"));
        Executor::shell("mkdir -pv /tmp/.breeze_theme && cd /tmp/.breeze_theme && "
            "curl -Lo index.html 'https://mirrors.bfsu.edu.cn/archlinuxcn/any/' 2>/dev/null && "
            "GREP_NAME='breeze-adapta-cursor-theme-git' && "
            "LATEST=$(cat index.html | grep \"$GREP_NAME\" | grep '.pkg.tar.zst' | tail -n1 | "
            "cut -d '=' -f3 | cut -d '\"' -f2) && "
            "[ -n \"$LATEST\" ] && curl -Lo data.tar.zst \"https://mirrors.bfsu.edu.cn/archlinuxcn/any/$LATEST\" 2>/dev/null && "
            "tar --use-compress-program zstd -xvf data.tar.zst 2>/dev/null && "
            "sudo cp -rf usr / 2>/dev/null || true");
        Executor::shell("rm -rf /tmp/.breeze_theme 2>/dev/null || true");
    }

    void DesktopManager::install_breeze_theme_ext() {
        Logger::step(_("gui.breeze.install"));
        download_arch_breeze_adapta_cursor_theme();
        auto family = resolved_family();
        if (family == DistroFamily::Arch) {
            PackageManager::install({"breeze-icons", "breeze-gtk", "xfwm4-theme-breeze", "capitaine-cursors"},
                                    DistroFamily::Arch);
        } else {
            Executor::passthrough(
                cfg_.install_command +
                " breeze-icon-theme breeze-cursor-theme breeze-gtk-theme xfwm4-theme-breeze 2>/dev/null || true");
        }
        Logger::ok(_("gui.breeze.ok"));
    }


    void DesktopManager::install_arc_gtk_theme_ext() {
        Executor::passthrough(cfg_.install_command + " arc-icon-theme 2>/dev/null || true");
        auto family = resolved_family();
        std::string arc_pkg = (family == DistroFamily::Arch) ? "arc-gtk-theme" : "arc-theme";
        Executor::passthrough(cfg_.install_command + " " + arc_pkg + " 2>/dev/null || true");
    }


    void DesktopManager::set_default_xfce_icon_theme(const std::string &icon_name) {
        Executor::shell(
            "dbus-launch xfconf-query -c xsettings -np /Net/IconThemeName -s " + icon_name + " 2>/dev/null || true");
        std::string home = SystemHelper::user_home();
        if (home != "/root") {
            // sudo 环境下 id -un 返回 root，优先用 $SUDO_USER 获取实际用户
            const char *sudo_user = std::getenv("SUDO_USER");
            std::string user = sudo_user ? sudo_user : Executor::shell("id -un").stdout_data;
            while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
            CommandBuilder("chown").add_flag("-Rv").add_arg(user + ":" + user).add_arg(home + "/.config/xfce4").add_raw(
                "2>/dev/null || true").execute();
        }
    }

    void DesktopManager::set_xfce_cursor_theme(const std::string &cursor_name) {
        Executor::shell(
            "dbus-launch xfconf-query -c xsettings -t string -np /Gtk/CursorThemeName -s " +
            cursor_name + " 2>/dev/null || true");
        std::string home = SystemHelper::user_home();
        if (home != "/root") {
            const char *sudo_user = std::getenv("SUDO_USER");
            std::string user = sudo_user ? sudo_user : Executor::shell("id -un").stdout_data;
            while (!user.empty() && (user.back() == '\n' || user.back() == '\r')) user.pop_back();
            CommandBuilder("chown").add_flag("-Rv").add_arg(user + ":" + user).add_arg(home + "/.config/xfce4").add_raw(
                "2>/dev/null || true").execute();
        }
    }

    void DesktopManager::tmoe_display_manager_systemctl(const std::string &dm_pkg, const std::string &dm_service) {
        while (true) {
            std::string menu = cfg_.tui_bin +
                               " --title \"" + _("gui.dm.menu_title") + "\" --menu \"" + _("gui.dm.menu_prompt") +
                               "\" 0 50 0 "
                               "\"1\" \"" + _("gui.dm.opt_install") + "\" "
                               "\"2\" \"" + _("gui.dm.opt_start") + "\" "
                               "\"3\" \"" + _("gui.dm.opt_stop") + "\" "
                               "\"4\" \"" + _("gui.dm.opt_enable") + "\" "
                               "\"5\" \"" + _("gui.dm.opt_disable") + "\" "
                               "\"0\" \"" + _("gui.dm.opt_back") + "\"";
            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) return;
            if (ch == "1") Executor::passthrough(cfg_.install_command + " " + dm_pkg + " 2>/dev/null || true");
            else if (ch == "2") Executor::passthrough(
                "sudo systemctl start " + dm_service + " 2>/dev/null || sudo service " + dm_service +
                " restart 2>/dev/null || true");
            else if (ch == "3") Executor::passthrough(
                "sudo systemctl stop " + dm_service + " 2>/dev/null || sudo service " + dm_service + " stop 2>/dev/null || true");
            else if (ch == "4") Executor::passthrough(
                "sudo systemctl enable " + dm_service + " 2>/dev/null || rc-update add " + dm_service +
                " 2>/dev/null || true");
            else if (ch == "5") Executor::passthrough(
                "sudo systemctl disable " + dm_service + " 2>/dev/null || rc-update del " + dm_service +
                " 2>/dev/null || true");
            Logger::press_enter();
        }
    }

    std::string DesktopManager::generate_update_icon_caches_script() {
        return gui_config::UPDATE_ICON_CACHES_SCRIPT;
    }

    void DesktopManager::create_update_icon_caches() {
        SystemHelper::write_file("/usr/local/bin/update-icon-caches", generate_update_icon_caches_script());
        CommandBuilder("sudo").add_arg("chmod").add_arg("a+rx").add_arg("/usr/local/bin/update-icon-caches").execute();
    }

    void DesktopManager::check_update_icon_caches_sh() {
        if (!Executor::has("update-icon-caches")) create_update_icon_caches();
    }

    void DesktopManager::download_xubuntu_wallpaper(const std::string &code_name, const std::string &) {
        Logger::step(_f("gui.wallpaper.xubuntu", code_name));
        std::string home = SystemHelper::user_home();
        std::string dest = home + "/Pictures/xubuntu-community-artwork";
        fs::create_directories(dest);
        SystemHelper::fetch_latest_and_extract(
            "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/x/xubuntu-community-artwork/",
            "xubuntu-community-wallpapers-" + code_name + ".*all\\.deb", "xubuntu_wp", dest);
    }

    void DesktopManager::download_mint_backgrounds(const std::string &mint_code) {
        Logger::step(_f("gui.wallpaper.mint", mint_code));
        std::string home = SystemHelper::user_home();
        std::string dest = home + "/Pictures/mint-backgrounds";
        fs::create_directories(dest);
        SystemHelper::fetch_latest_and_extract(
            "https://mirrors.bfsu.edu.cn/linuxmint/pool/main/m/mint-backgrounds-" + mint_code + "/",
            "mint-backgrounds.*all\\.deb", "mint_bg", dest);
    }

    void DesktopManager::download_kali_themes_common() {
        check_update_icon_caches_sh();
        // 主源 → 回退源
        if (!SystemHelper::fetch_latest_and_extract(
                "https://mirrors.bfsu.edu.cn/kali/pool/main/k/kali-themes/",
                "kali-themes-common.*all\\.deb", "kali_themes"))
            SystemHelper::fetch_latest_and_extract(
                "http://http.kali.org/kali/pool/main/k/kali-themes/",
                "kali-themes-common.*all\\.deb", "kali_themes_fb");
        Executor::shell(
            "sudo update-icon-caches /usr/share/icons/Flat-Remix-Blue-Dark /usr/share/icons/Flat-Remix-Blue-Light /usr/share/icons/desktop-base 2>/dev/null &");
        set_xfce_cursor_theme("Breeze-Adapta-Cursor");
    }

    void DesktopManager::download_kali_theme() {
        if (!fs::exists("/usr/share/desktop-base/kali-theme")) download_kali_themes_common();
        else {
            Logger::info(_("gui.kali.themes_downloaded"));
            download_kali_themes_common();
        }
        set_default_xfce_icon_theme("Flat-Remix-Blue-Light");
    }

    void DesktopManager::download_papirus_icon_theme() {
        Logger::step(_("gui.papirus.install"));
        SystemHelper::fetch_latest_and_extract(
            "https://mirrors.bfsu.edu.cn/debian/pool/main/p/papirus-icon-theme/",
            "papirus-icon-theme.*all\\.deb", "papirus");
        Executor::shell(
            "sudo update-icon-caches /usr/share/icons/Papirus /usr/share/icons/Papirus-Dark /usr/share/icons/Papirus-Light /usr/share/icons/ePapirus 2>/dev/null &");
        set_default_xfce_icon_theme("Papirus");
    }

    void DesktopManager::install_moka_theme_ext() {
        Executor::passthrough(cfg_.install_command + " moka-icon-theme 2>/dev/null || true");
    }

    void DesktopManager::install_numix_theme_ext() {
        Executor::passthrough(cfg_.install_command + " numix-gtk-theme 2>/dev/null || true");
        auto family = resolved_family();
        std::string circle_pkg = (family == DistroFamily::Arch)
                                     ? "numix-circle-icon-theme-git"
                                     : "numix-icon-theme-circle";
        Executor::passthrough(cfg_.install_command + " " + circle_pkg + " 2>/dev/null || true");
    }

    void DesktopManager::download_ubuntu_mate_wallpaper() {
        Logger::step(_("gui.wallpaper.ubuntu_mate"));
        SystemHelper::fetch_latest_and_extract(
            "https://mirrors.bfsu.edu.cn/ubuntu/pool/universe/u/ubuntu-mate-artwork/",
            "ubuntu-mate-wallpapers-photos.*all\\.deb", "umate_wp");
    }

    // ═══════════════════════════════════════════════════════════════
    // XFCE 壁纸自动配置（移植自 bash: modify_the_default_xfce_wallpaper）
    // ═══════════════════════════════════════════════════════════════

    namespace {
        // Mint 各版本壁纸数据 — 对应 linuxmint_*_wallpaper_var() ×10
        struct MintWallpaper {
            const char *code;
            const char *path;
        };
        constexpr MintWallpaper kMintWallpapers[] = {
            {"serena", "/usr/share/backgrounds/rlukeman_skye.jpg"},
            {"sonya",  "/usr/share/backgrounds/shontz_valley.jpg"},
            {"sylvia", "/usr/share/backgrounds/thomasb_glass_ball.jpg"},
            {"tara",   "/usr/share/backgrounds/jowens_kauai.jpg"},
            {"tessa",  "/usr/share/backgrounds/dking_autumn_in_japan.jpg"},
            {"tina",   "/usr/share/backgrounds/adeole_yosemite.jpg"},
            {"tricia", "/usr/share/backgrounds/amarttinen_argentina.jpg"},
            {"ulyana", "/usr/share/backgrounds/dmcquade_whitsundays.jpg"},
            {"ulyssa", "/usr/share/backgrounds/sumstattd_machu_picchu.jpg"},
            {"sarah",  "/usr/share/backgrounds/bartosova_aurora.jpg"},
        };
        constexpr int kMintCount = sizeof(kMintWallpapers) / sizeof(kMintWallpapers[0]);
    }

    std::pair<std::string, std::string> DesktopManager::select_random_mint_wallpaper() const {
        // 对应 random_wallpaper_pack_01~05 — 用不同权重映射到 Mint 壁纸
        // 简化：直接随机均匀选择
        int r = std::rand() % kMintCount;
        return {kMintWallpapers[r].code, kMintWallpapers[r].path};
    }

    std::string DesktopManager::select_distro_wallpaper() const {
        // 对应 modify_the_default_xfce_wallpaper() 的 case 分支
        auto fam = resolved_family();

        switch (fam) {
        case DistroFamily::Debian: {
            auto [code, path] = select_random_mint_wallpaper();
            // Kali: 把 kali-16x9 壁纸复制到 backgrounds
            if (fs::exists("/usr/share/backgrounds/kali-16x9")) {
                Executor::shell("sudo cp -sv /usr/share/backgrounds/kali-16x9/* "
                                "/usr/share/backgrounds/ 2>/dev/null || true");
            }
            return path;
        }
        case DistroFamily::Arch:
            // 把 xfce 子目录壁纸移到顶层
            if (fs::exists("/usr/share/backgrounds/xfce")) {
                Executor::shell("sudo mv -f /usr/share/backgrounds/xfce/* "
                                "/usr/share/backgrounds/ 2>/dev/null || true");
            }
            return select_random_mint_wallpaper().second;
        case DistroFamily::RedHat:
            return select_random_mint_wallpaper().second;
        default: {
            // 其他发行版: random_pack_05 — 包含 ubuntu_mate 壁纸
            int r = std::rand() % 31;
            if (r >= 23 && r <= 27) {
                // ubuntu_mate_wallpaper_var
                return "/usr/share/backgrounds/johann-siemens-591.jpg";
            }
            return select_random_mint_wallpaper().second;
        }
        }
    }

    void DesktopManager::ensure_xfce_wallpaper_config(const std::string &wallpaper_path) {
        // 对应 create_xfce4_desktop_wallpaper_config() + modify_xfce_vnc0_wallpaper()
        std::string home = SystemHelper::user_home();
        fs::path config_dir = fs::path(home) / ".config" / "xfce4" / "xfconf" / "xfce-perchannel-xml";
        fs::path desktop_xml = config_dir / "xfce4-desktop.xml";

        fs::create_directories(config_dir);
        SystemHelper::write_file(desktop_xml, gui_config::xfce_desktop_xml(wallpaper_path));
        Logger::info(std::string(_("gui.desktop.xfce_wallpaper_config_created")) + desktop_xml.string());
    }

    void DesktopManager::configure_default_xfce_wallpaper() {
        // 对应 modify_the_default_xfce_wallpaper() — DE 安装后的入口
        Logger::step(_("gui.desktop.configuring_wallpaper"));

        std::string wallpaper_path = select_distro_wallpaper();

        // 检查壁纸是否存在，否则触发下载
        if (!fs::exists(wallpaper_path)) {
            Logger::info(std::string(_("gui.desktop.wallpaper_missing_download")) + wallpaper_path);

            auto fam = resolved_family();
            auto [mint_code, _] = select_random_mint_wallpaper();
            download_mint_backgrounds(mint_code);

            // 如果壁纸仍未就绪，尝试 ubuntu-mate 壁纸
            if (!fs::exists(wallpaper_path)) {
                download_ubuntu_mate_wallpaper();
                wallpaper_path = "/usr/share/backgrounds/johann-siemens-591.jpg";
            }
        }

        // 生成 xfce4-desktop.xml 配置
        ensure_xfce_wallpaper_config(wallpaper_path);

        Logger::ok(_("gui.desktop.wallpaper_configured"));
    }
} // namespace tmoe::domain
