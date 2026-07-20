/** 双按钮选择操作 — IAction 子类。
 *
 *  封装 whiptail --yesno 对话框模式：用户看到两个自定义按钮，
 *  选择其中之一触发对应的回调。
 *
 *  典型用法（浏览器安装/卸载）：
 *      menu->add_child(std::make_shared<ChoiceAction>(
 *          _("label"), "tag",
 *          _("title"), _("text"),
 *          "install", [this]{ mgr->install_xxx(); },
 *          "remove",  [this]{ mgr->remove_xxx(); }));
 */
#pragma once
#include "ui/action.h"
#include "ui/dialog_helpers.h"
#include <functional>

namespace tmoe::ui {

class ChoiceAction : public IAction {
public:
    /** @param label     菜单项显示文本
     *  @param tag       菜单项标识符
     *  @param title     对话框标题
     *  @param text      对话框提示文本
     *  @param yes_label Yes 按钮标签
     *  @param yes_fn    Yes 回调
     *  @param no_label  No 按钮标签
     *  @param no_fn     No 回调
     *  @param height    对话框高度（0 = 自动）
     *  @param width     对话框宽度（0 = 自动）
     */
    ChoiceAction(std::string label, std::string tag,
                 std::string title, std::string text,
                 std::string yes_label, std::function<void()> yes_fn,
                 std::string no_label, std::function<void()> no_fn,
                 int height = 0, int width = 0)
        : label_(std::move(label)), tag_(std::move(tag)),
          title_(std::move(title)), text_(std::move(text)),
          yes_label_(std::move(yes_label)), yes_fn_(std::move(yes_fn)),
          no_label_(std::move(no_label)), no_fn_(std::move(no_fn)),
          height_(height), width_(width) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }

    bool execute(MenuContext& ctx) override {
        int c = dialog::yesno(ctx.cfg, title_, text_,
                              yes_label_, no_label_, height_, width_);
        if (c == 0 && yes_fn_) {
            yes_fn_();
            return true;
        }
        if (c == 1 && no_fn_) {
            no_fn_();
            return true;
        }
        return c != 255; // ESC = 用户取消
    }

    bool needs_root() const override { return false; }

private:
    std::string label_, tag_, title_, text_;
    std::string yes_label_, no_label_;
    std::function<void()> yes_fn_, no_fn_;
    int height_, width_;
};

} // namespace tmoe::ui
