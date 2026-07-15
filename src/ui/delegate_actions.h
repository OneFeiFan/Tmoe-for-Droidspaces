#pragma once
#include "ui/action.h"
#include "ui/menu.h"
#include <functional>

namespace tmoe::ui {

/**
 * 委托操作 — 将旧式 std::function 回调包装为 IAction。
 * Phase 2 迁移期使用，新旧代码的适配器。
 */
class DelegateAction : public IAction {
public:
    using Callback = std::function<void()>;

    DelegateAction(std::string label, std::string tag,
                   Callback cb, bool root = false)
        : label_(std::move(label)), tag_(std::move(tag)),
          callback_(std::move(cb)), needs_root_(root) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    bool execute(MenuContext&) override { callback_(); return true; }
    bool needs_root() const override { return needs_root_; }

private:
    std::string label_;
    std::string tag_;
    Callback callback_;
    bool needs_root_;
};

/**
 * 委托子菜单 — 允许旧式子菜单循环包装为 IUIMenu。
 * 传入一个渲染函数（返回 whiptail 标签）和分发函数。
 */
class DelegateMenu : public IUIMenu {
public:
    using Renderer = std::function<std::string()>;
    using Dispatcher = std::function<bool(const std::string&)>;

    DelegateMenu(std::string title, std::string label, std::string tag,
                 Renderer renderer, Dispatcher dispatcher)
        : title_(std::move(title)), label_(std::move(label)),
          tag_(std::move(tag)), renderer_(std::move(renderer)),
          dispatcher_(std::move(dispatcher)) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    std::string get_title() const override { return title_; }

    /** 委托菜单不使用默认的 children 机制，
     *  而是直接调用旧式渲染+分发循环。*/
    std::string build_whiptail_cmd(const MenuContext& ctx) const override {
        (void)ctx;
        return renderer_();
    }

    /** 分发到旧式 dispatcher。*/
    bool dispatch(const std::string& choice) {
        return dispatcher_(choice);
    }

private:
    std::string title_;
    std::string label_;
    std::string tag_;
    Renderer renderer_;
    Dispatcher dispatcher_;
};

} // namespace tmoe::ui
