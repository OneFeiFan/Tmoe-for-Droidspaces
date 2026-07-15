#pragma once
#include "ui/menu.h"
#include "ui/action.h"
#include "ui/registry.h"
#include "ui/builtin_actions.h"
#include "core/logger.h"
#include "core/i18n.h"

namespace tmoe::ui {

// ── LambdaAction ─────────────────────────────────────

/** 通用 IAction 实现——将任意 lambda 包装为菜单操作。
 *  替代 DelegateAction，用于无需自定义类的简单场景。 */
class LambdaAction : public IAction {
public:
    using ExecFn = std::function<bool(MenuContext&)>;

    LambdaAction(std::string label, std::string tag,
                 ExecFn exec, bool root = false)
        : label_(std::move(label)), tag_(std::move(tag)),
          exec_(std::move(exec)), needs_root_(root) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    bool execute(MenuContext& ctx) override { return exec_ ? exec_(ctx) : false; }
    bool needs_root() const override { return needs_root_; }

    /** 快捷工厂：创建无需 MenuContext 的简单操作。 */
    static std::shared_ptr<LambdaAction> make(
        std::string label, std::string tag,
        std::function<void()> fn, bool root = false)
    {
        return std::make_shared<LambdaAction>(
            std::move(label), std::move(tag),
            [fn = std::move(fn)](MenuContext&) -> bool { fn(); return true; },
            root);
    }

private:
    std::string label_, tag_;
    ExecFn exec_;
    bool needs_root_;
};

// ── AutoRegister ─────────────────────────────────────

/** RAII 自动注册器。
 *  在静态初始化阶段（main 之前）将菜单项注册到 MenuRegistry。
 *
 *  使用方式：
 *      static AutoRegister reg(std::make_shared<BrowserMenu>());
 */
class AutoRegister {
public:
    explicit AutoRegister(std::shared_ptr<IMenuItem> item) {
        MenuRegistry::register_item(std::move(item));
    }
};

// ── 插件构建辅助 ──────────────────────────────────────

/** 快捷创建 SimpleMenu（不含导航项，由调用方在所有内容之后添加）。 */
inline std::shared_ptr<SimpleMenu> make_plugin_menu(
    std::string title, std::string label, std::string tag)
{
    return std::make_shared<SimpleMenu>(
        std::move(title), std::move(label), std::move(tag));
}

/** 创建"安装 → 确认 → Logger 输出"模式的 LambdaAction。 */
inline std::shared_ptr<LambdaAction> make_install_action(
    std::string label, std::string tag,
    std::function<bool()> install_fn)
{
    // 捕获 label 副本进 lambda，避免 move 后悬空
    auto display_label = label;
    return std::make_shared<LambdaAction>(
        std::move(label), std::move(tag),
        [fn = std::move(install_fn), display_label](MenuContext&) -> bool {
            Logger::step(_("app.installing") + std::string(": ") + display_label);
            bool ok = fn();
            if (ok) Logger::ok(_("app.install_done"));
            else Logger::error(_("app.install_failed"));
            Logger::press_enter();
            return ok;
        });
}

} // namespace tmoe::ui
