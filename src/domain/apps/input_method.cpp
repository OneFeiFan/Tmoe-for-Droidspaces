#include "domain/apps/input_method.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/logger.h"
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {
    InputMethodManager::InputMethodManager(const TmoeConfig &cfg) : cfg_(cfg) {
    }

    void InputMethodManager::run_input_method_menu() {
        while (true) {
            std::string menu = cfg_.tui_bin + " --title \"" + _("input.menu_title")
                               + "\" --menu \"" + _("input.menu_prompt") + "\" 0 0 0 "
                               "\"1\" \"" + _("input.fcitx4") + "\" "
                               "\"2\" \"" + _("input.fcitx5") + "\" "
                               "\"3\" \"" + _("input.ibus") + "\" "
                               "\"4\" \"" + _("input.sogou") + "\" "
                               "\"5\" \"" + _("input.faq") + "\" "
                               "\"0\" \"" + _("menu.tui.back_upper") + "\"";

            auto ch = Executor::tui_select(menu);
            if (ch == "0" || ch.empty()) break;
            if (ch == "1") run_fcitx4_menu();
            else if (ch == "2") run_fcitx5_menu();
            else if (ch == "3") run_ibus_menu();
            else if (ch == "4") install_sogou();
            else if (ch == "5") show_input_faq();
            Logger::press_enter();
        }
    }

    void InputMethodManager::run_fcitx4_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("input.fcitx4_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("input.fcitx4_googlepinyin") + "\" "
                           "\"2\" \"" + _("input.fcitx4_rime") + "\" "
                           "\"3\" \"" + _("input.fcitx4_libpinyin") + "\" "
                           "\"4\" \"" + _("input.fcitx4_sunpinyin") + "\" "
                           "\"5\" \"" + _("input.fcitx4_cloudpinyin") + "\" "
                           "\"6\" \"" + _("input.fcitx4_kde_config") + "\" "
                           "\"7\" \"" + _("input.fcitx4_tools") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        // 确保基础 fcitx 已安装
        PackageManager::install({"fcitx", "fcitx-config-gtk", "fcitx-im"}, family);

        if (ch == "1") PackageManager::install("fcitx-googlepinyin", family);
        else if (ch == "2") PackageManager::install("fcitx-rime", family);
        else if (ch == "3") PackageManager::install("fcitx-libpinyin", family);
        else if (ch == "4") PackageManager::install("fcitx-sunpinyin", family);
        else if (ch == "5") PackageManager::install("fcitx-module-cloudpinyin", family);
        else if (ch == "6") PackageManager::install("kde-config-fcitx", family);
        else if (ch == "7")
            PackageManager::install({
                                        "fcitx", "fcitx-tools", "fcitx-config-gtk",
                                        "fcitx-im", "fcitx-configtool", "fcitx-googlepinyin", "fcitx-rime",
                                        "fcitx-libpinyin", "fcitx-sunpinyin"
                                    }, family);

        write_input_env_vars();
        setup_fcitx_autostart();
        Logger::press_enter();
    }

    void InputMethodManager::run_fcitx5_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("input.fcitx5_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("input.fcitx5_core") + "\" "
                           "\"2\" \"" + _("input.fcitx5_qt_gtk") + "\" "
                           "\"3\" \"" + _("input.fcitx5_kcm") + "\" "
                           "\"4\" \"" + _("input.fcitx5_rime") + "\" "
                           "\"5\" \"" + _("input.fcitx5_material") + "\" "
                           "\"6\" \"" + _("input.fcitx5_kimpanel") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        if (ch == "1") PackageManager::install({"fcitx5", "fcitx5-chinese-addons"}, family);
        else if (ch == "2") PackageManager::install({"fcitx5-qt", "fcitx5-gtk"}, family);
        else if (ch == "3") PackageManager::install("kcm-fcitx5", family);
        else if (ch == "4") PackageManager::install("fcitx5-rime", family);
        else if (ch == "5") PackageManager::install("fcitx5-material-color", family);
        else if (ch == "6") {
            PackageManager::install({"fcitx5-module-kimpanel", "gnome-shell-extension-kimpanel"}, family);
        }
        write_input_env_vars();
        Logger::press_enter();
    }

    void InputMethodManager::run_ibus_menu() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        std::string menu = cfg_.tui_bin + " --title \"" + _("input.ibus_menu")
                           + "\" --menu \"\" 0 0 0 "
                           "\"1\" \"" + _("input.ibus_pinyin") + "\" "
                           "\"2\" \"" + _("input.ibus_rime") + "\" "
                           "\"3\" \"" + _("input.ibus_sunpinyin") + "\" "
                           "\"4\" \"" + _("input.ibus_chewing") + "\" "
                           "\"5\" \"" + _("input.ibus_onboard") + "\" "
                           "\"0\" \"" + _("menu.tui.back") + "\"";
        auto ch = Executor::tui_select(menu);
        if (ch == "0" || ch.empty()) return;

        PackageManager::install("ibus", family);
        if (ch == "1") PackageManager::install("ibus-libpinyin", family);
        else if (ch == "2") PackageManager::install("ibus-rime", family);
        else if (ch == "3") PackageManager::install("ibus-sunpinyin", family);
        else if (ch == "4") PackageManager::install("ibus-chewing", family);
        else if (ch == "5") PackageManager::install("onboard", family);
        Logger::press_enter();

        // ibus 自动启动
        Logger::step(_("input.autostart"));
        Executor::shell(
            "if ! pgrep -x ibus-daemon >/dev/null 2>&1; then "
            "  ibus-daemon -drx --panel=/usr/lib/ibus/ibus-ui-gtk3 2>/dev/null || "
            "  ibus-daemon -drx 2>/dev/null || true; fi"
        );
    }

    void InputMethodManager::install_sogou() {
        auto family = infer_family_from_config(cfg_.linux_distro);
        Logger::step(_("input.sogou"));
        PackageManager::install("sogoupinyin", family);
        write_input_env_vars();
    }

    void InputMethodManager::write_input_env_vars() {
        Logger::step(_("input.env_writing"));
        std::string env_block =
                "GTK_IM_MODULE=fcitx\n"
                "QT_IM_MODULE=fcitx\n"
                "XMODIFIERS=@im=fcitx\n"
                "SDL_IM_MODULE=fcitx\n";
        Executor::shell("echo '" + env_block + "' >> /etc/environment 2>/dev/null || true");
        Logger::ok(_("input.env_done"));
    }

    void InputMethodManager::setup_fcitx_autostart() {
        Logger::step(_("input.autostart"));
        Executor::shell("mkdir -p /etc/xdg/autostart ~/.config/autostart 2>/dev/null");
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
} // namespace tmoe::domain
