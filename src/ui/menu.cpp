#include "menu.h"
#include "core/command_builder.hpp"
#include "core/config.h"
#include <algorithm>

namespace tmoe::ui {

void IUIMenu::add_child(std::shared_ptr<IMenuItem> item) {
    children_.push_back(std::move(item));
}

void IUIMenu::add_children(std::initializer_list<std::shared_ptr<IMenuItem>> items) {
    for (auto& item : items) {
        children_.push_back(item);
    }
}

std::shared_ptr<IMenuItem> IUIMenu::find_child(const std::string& tag) const {
    for (const auto& child : children_) {
        if (child->get_tag() == tag) return child;
    }
    return nullptr;
}

std::string IUIMenu::build_whiptail_cmd(const MenuContext& ctx) const {
    std::string cur_lang = "C"; // TmoeConfig locale 或默认
    std::string title = get_title();

    CommandBuilder cmd(ctx.cfg.tui_bin);
    cmd.add_env("LANG", cur_lang + ".UTF-8");
    cmd.add_arg("--title").add_arg(title);
    cmd.add_arg("--menu").add_arg(get_prompt());

    // whiptail/dialog 标准参数：0=自动高度, 0=自动宽度, 0=自动菜单高度
    cmd.add_arg("0").add_arg("0").add_arg("0");

    // 为每个可见子项生成 "tag" "label" 对
    for (const auto& child : children_) {
        if (!child->is_visible(ctx)) continue;
        cmd.add_arg(child->get_tag());
        cmd.add_arg(child->get_label());
    }

    return cmd.build_string();
}

} // namespace tmoe::ui
