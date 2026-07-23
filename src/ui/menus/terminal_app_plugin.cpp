/** TerminalApp 菜单插件 — 每个终端应用一个 LambdaAction，直接调用安装方法。 */
#include "ui/menus/terminal_app_plugin.h"
#include "ui/plugin_helpers.h"
#include "ui/builtin_actions.h"
#include "domain/apps/terminal_app.h"

namespace tmoe::ui::menus {

    std::shared_ptr<IUIMenu> TerminalAppMenuPlugin::build() {
        auto menu = make_plugin_menu(
                _("term.menu_title"), _("term.menu_prompt"), "plugin_terminal_app");

        // 对应旧 Bash terminal (87行) tmoe_terminal_main_menu — 17 项
        // 每项原为 whiptail radio 条目，现映射为独立 LambdaAction

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.cool_retro_term"), "install_cool_retro_term",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("cool-retro-term", "term.cool_retro_term",
                                           "term.hint_cool_retro_term");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.terminology"), "install_terminology",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("terminology", "term.terminology",
                                           "term.hint_terminology");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.terminator"), "install_terminator",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("terminator", "term.terminator",
                                           "term.hint_terminator");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.yakuake"), "install_yakuake",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("yakuake", "term.yakuake");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.guake"), "install_guake",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("guake", "term.guake");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.tilda"), "install_tilda",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("tilda", "term.tilda");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.konsole"), "install_konsole",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("konsole", "term.konsole");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.xfce4_terminal"), "install_xfce4_terminal",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("xfce4-terminal", "term.xfce4_terminal");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.gnome_terminal"), "install_gnome_terminal",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("gnome-terminal", "term.gnome_terminal");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.lxterminal"), "install_lxterminal",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("lxterminal", "term.lxterminal");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.mate_terminal"), "install_mate_terminal",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("mate-terminal", "term.mate_terminal");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.lilyterm"), "install_lilyterm",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("lilyterm", "term.lilyterm");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.sakura"), "install_sakura",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("sakura", "term.sakura");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.rxvt"), "install_rxvt",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("rxvt", "term.rxvt");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.st"), "install_st",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("stterm", "term.st",
                                           "term.hint_st");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.aterm"), "install_aterm",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("aterm", "term.aterm",
                                           "term.hint_aterm");
                    return true;
                }));

        menu->add_child(std::make_shared<LambdaAction>(
                _("term.xterm"), "install_xterm",
                [this](MenuContext &) -> bool {
                    mgr_->install_terminal("xterm", "term.xterm");
                    return true;
                }));

        add_sandwich_nav(menu);
        return menu;
    }

} // namespace tmoe::ui::menus
