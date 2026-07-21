#include "domain/apps/input_method.h"
#include "core/command_builder.hpp"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    InputMethodManager::InputMethodManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void InputMethodManager::install_sogou() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("input.sogou"));
        PackageManager::install("sogoupinyin", family);
        write_input_env_vars();
    }

    void InputMethodManager::write_input_env_vars() {
        // 避免重复追加：先检查是否已有 fcitx 环境变量
        auto check = Executor::shell(
            "grep -q 'GTK_IM_MODULE=fcitx' /etc/environment 2>/dev/null && echo found || true");
        if (check.ok() && check.stdout_data.find("found") != std::string::npos) {
            Logger::info(_("input.env_already_set"));
            return;
        }
        Logger::step(_("input.env_writing"));
        std::string env_block =
                "GTK_IM_MODULE=fcitx\n"
                "QT_IM_MODULE=fcitx\n"
                "XMODIFIERS=@im=fcitx\n"
                "SDL_IM_MODULE=fcitx\n";
        Executor::shell(CommandBuilder("echo").add_arg(env_block).add_raw("| sudo tee -a /etc/environment >/dev/null 2>&1 || true").build_string());
        Logger::ok(_("input.env_done"));
    }

    void InputMethodManager::setup_fcitx_autostart() {
        Logger::step(_("input.autostart"));
        Executor::shell(CommandBuilder("sudo").add_arg("mkdir").add_flag("-p").add_arg("/etc/xdg/autostart").add_raw("2>/dev/null").build_string());
        Executor::shell(CommandBuilder("mkdir").add_flag("-p").add_arg("~/.config/autostart").add_raw("2>/dev/null").build_string());
        // 复制 fcitx 自动启动到 XDG 目录（如果存在）
        Executor::shell(
            "if [ -f /etc/xdg/autostart/fcitx-autostart.desktop ]; then "
            "  cp /etc/xdg/autostart/fcitx-autostart.desktop "
            "     ~/.config/autostart/ 2>/dev/null; fi"
        );
    }

    void InputMethodManager::show_input_faq() {
        Logger::info("══════════ " + _("input.faq") + " ══════════\n");
        Logger::info(_("input.faq_1"));
        Logger::info(_("input.faq_2"));
        Logger::info(_("input.faq_3"));
        Logger::info(_("input.faq_4"));
        Logger::info(_("input.faq_5"));
        Logger::info(_("input.faq_6"));
    }

    // ── 插件子菜单所需的细粒度操作 ──────────────────────

    void InputMethodManager::install_fcitx4_engine(const std::string& pkg_name) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install({"fcitx", "fcitx-config-gtk", "fcitx-im"}, family);
        PackageManager::install(pkg_name, family);
        write_input_env_vars();
        setup_fcitx_autostart();
    }

    void InputMethodManager::install_fcitx4_tools() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install({
            "fcitx", "fcitx-tools", "fcitx-config-gtk",
            "fcitx-im", "fcitx-configtool", "fcitx-googlepinyin", "fcitx-rime",
            "fcitx-libpinyin", "fcitx-sunpinyin"
        }, family);
        write_input_env_vars();
        setup_fcitx_autostart();
    }

    void InputMethodManager::install_fcitx5_packages(const std::vector<std::string>& pkgs) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install(pkgs, family);
        write_input_env_vars();
    }

    void InputMethodManager::install_ibus_engine(const std::string& pkg_name) {
        auto family = infer_family_from_config(cfg_.linux_distro);
        PackageManager::install("ibus", family);
        PackageManager::install(pkg_name, family);
        // ibus 自动启动
        Logger::step(_("input.autostart"));
        Executor::shell(
            "if ! pgrep -x ibus-daemon >/dev/null 2>&1; then "
            "  ibus-daemon -drx --panel=/usr/lib/ibus/ibus-ui-gtk3 2>/dev/null || "
            "  ibus-daemon -drx 2>/dev/null || true; fi"
        );
    }
} // namespace tmoe::domain
