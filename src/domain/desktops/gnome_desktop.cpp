#include "gnome_desktop.h"
#include "desktop_utils.h"
#include "core/command_builder.hpp"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "ui/menu_engine.h"

namespace tmoe::domain {
    SessionCmds GnomeDesktop::get_session_commands() const {
        if (session_ == "2") return {"gnome-flashback-metacity", "gnome-shell-x11"};
        if (session_ == "4") return {"gnome-session-ubuntu", "gnome-session"};
        if (session_ == "5") return {"gnome-session-classic", "gnome-session"};
        return {"gnome-shell-x11", "gnome-flashback-metacity"};
    }

    PreInstallChoices GnomeDesktop::pre_install_choices(DistroFamily f, bool a) {
        PreInstallChoices c;
        if (a) return c;
        if (f == DistroFamily::Debian) {
            using namespace tmoe::ui;
            auto menu = make_plugin_menu("gnome-session", "Select GNOME session", "gnome_session");
            menu->add_child(std::make_shared<LambdaAction>("gnome-shell-x11", "1", [this](MenuContext&) -> bool {
                session_ = "1"; return false;
            }));
            menu->add_child(std::make_shared<LambdaAction>("gnome-flashback", "2", [this](MenuContext&) -> bool {
                session_ = "2"; return false;
            }));
            menu->add_child(std::make_shared<LambdaAction>("gnome-session", "3", [this](MenuContext&) -> bool {
                session_ = "3"; return false;
            }));
            menu->add_child(std::make_shared<LambdaAction>("gnome-session-ubuntu", "4", [this](MenuContext&) -> bool {
                session_ = "4"; return false;
            }));
            menu->add_child(std::make_shared<LambdaAction>("gnome-session-classic", "5", [this](MenuContext&) -> bool {
                session_ = "5"; return false;
            }));
            menu->add_child(std::make_shared<LambdaAction>("Back", "0", [this](MenuContext&) -> bool {
                session_ = "0"; return false;
            }));
            MenuContext ctx{const_cast<TmoeConfig&>(cfg_)};
            MenuEngine(ctx).run(menu);

            if (session_ == "2") {
                c.pkg_list = "gnome-panel gnome-menus gnome-shell gnome-session-flashback gnome-session";
                return c;
            }
            if (session_.empty() || session_ == "0") return c;
            if (cfg_.sub_distro == "ubuntu") {
                auto r = Executor::passthrough(
                    cfg_.tui_bin +
                    " --title \"gnome/ubuntu\" --yes-button \"gnome\" --no-button \"ubuntu-desktop\" --yesno 'gnome/ubuntu-desktop?' 0 0");
                if (r.exit_code == 1) {
                    c.pkg_list = "ubuntu-desktop";
                    return c;
                }
            }
            if (c.pkg_list.empty()) {
                auto r = Executor::passthrough(
                    cfg_.tui_bin +
                    " --title \"gnome-shell/core\" --yes-button \"gnome-shell\" --no-button \"gnome-core\" --yesno 'shell/core?' 0 0");
                c.pkg_list = (r.exit_code == 0)
                                 ? "xorg gnome-panel gnome-menus gnome-shell gnome-session"
                                 : "gnome-core";
            }
        } else if (f == DistroFamily::Arch) {
            auto r = Executor::passthrough(
                cfg_.tui_bin +
                " --title \"gnome/gnome-extra\" --yes-button \"gnome\" --no-button \"gnome-extra\" --yesno 'gnome/extra?' 0 0");
            c.pkg_list = (r.exit_code == 0) ? "gnome-tweaks gnome" : "gnome-extra gnome";
        } else if (f == DistroFamily::RedHat) {
            c.pkg_list = "@GNOME";
        }
        return c;
    }

    void GnomeDesktop::will_be_installed_message() const {
        Logger::info("GNOME: gnome-session / gnome-shell-x11");
        Logger::info(_("gui.gnome.package_list"));
        auto family = infer_family_from_config(cfg_.linux_distro);
        if (family == DistroFamily::Unknown)
            family = PackageManager::detect_distro_family();
        if (family == DistroFamily::Debian && Executor::has("apt-cache"))
            Executor::passthrough("apt-cache depends gnome-session 2>/dev/null | head -20 || true");
        else if (family == DistroFamily::Arch && Executor::has("pacman"))
            Executor::passthrough("pacman -Si gnome-shell 2>/dev/null || true");
    }

    void GnomeDesktop::post_install_config(const PostInstallContext &ctx) {
        if (!gnome_warning()) return;
        Logger::info("GNOME desktop install");
        desktop_utils::dpkg_configure_and_keyboard(ctx.is_debian);
        desktop_utils::purge_libfprint_and_clean(ctx.is_proot, ctx.is_debian);
        if (ctx.is_debian) {
            desktop_utils::install_noto_fonts(ctx.family, true);
        }
        write_session_scripts();
        desktop_utils::install_language_packs(cfg_);
    }

    void GnomeDesktop::write_session_scripts() {
        auto wr = [&](const char *n, const std::string &c) {
            SystemHelper::write_file("/usr/local/bin/" + std::string(n), c);
            CommandBuilder("sudo").add_arg("chmod").add_arg("a+rx").add_arg("/usr/local/bin/" + std::string(n)).execute();
        };
        // bash: gnome-shell-x11 总是创建（不管选哪个 session）
        wr("gnome-shell-x11", "#!/bin/sh\nexec gnome-shell --x11 \"$@\"\n");
        if (session_ == "2") {
            // bash: ln_s_gnome_flashback_metacity — 检测 --builtin 支持，不支持则退回 metacity &
            std::string fb = "#!/bin/sh\nexport XDG_CURRENT_DESKTOP=\"GNOME-Flashback:GNOME\"\n"
                    "if gnome-session --help 2>&1 | grep -q builtin; then\n"
                    "  exec gnome-session --builtin --session=gnome-flashback-metacity --disable-acceleration-check \"$@\"\n"
                    "else\n  metacity &\n  exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check \"$@\"\nfi\n";
            wr("gnome-flashback-metacity", fb);
        }
        if (session_ == "4") wr("gnome-session-ubuntu",
                                "#!/bin/sh\nexport DESKTOP_SESSION=ubuntu\nexport GNOME_SHELL_SESSION_MODE=ubuntu\nexport XDG_CURRENT_DESKTOP=\"ubuntu:GNOME\"\nexport XDG_CONFIG_DIRS=/etc/xdg/xdg-ubuntu:/etc/xdg\nexec gnome-session --session=ubuntu --disable-acceleration-check \"$@\"\n");
        if (session_ == "5") wr("gnome-session-classic",
                                "#!/bin/sh\nexport GNOME_SHELL_SESSION_MODE=classic\nexec gnome-session --session=gnome-classic --disable-acceleration-check \"$@\"\n");
    }
void GnomeDesktop::post_install_extras(const PostInstallContext& ctx) {
    // GNOME tweaks 和扩展（Bash 原版在 GNOME 安装后安装）
    auto family = ctx.family;
    PackageManager::install({"gnome-tweaks", "gnome-shell-extensions"}, family);
    // chrome-gnome-shell 用于浏览器集成安装扩展
    if (family == DistroFamily::Debian) {
        PackageManager::install("chrome-gnome-shell", family);
    }
    // GNOME 壁纸
    if (ctx.is_debian && ctx.is_ubuntu) {
        PackageManager::install("ubuntu-wallpapers", family);
    }
}

bool GnomeDesktop::gnome_warning() const {
    Logger::info("------------------------");
    Logger::info(_("gui.gnome.warning.continue"));
    if (!Logger::confirm_yes_default(_("gui.gnome.warning.continue"))) {
        Logger::warn(_("gui.gnome.warning.cancelled"));
        return false;
    }
    return true;
}

} // namespace tmoe::domain
