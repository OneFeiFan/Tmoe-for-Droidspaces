#pragma once
#include "core/config.h"
#include <string>
#include <memory>
#include <vector>

namespace tmoe::ui {

/** 传递给菜单项渲染和执行时的上下文信息。 */
struct MenuContext {
    TmoeConfig& cfg;

    explicit MenuContext(TmoeConfig& c) : cfg(c) {}
};

/**
 * 菜单项抽象基类。
 *
 * IUIMenu（容器）和 IAction（叶子）的共同基类，
 * 提供标签、标识符和条件可见性三个核心属性。
 */
class IMenuItem {
public:
    virtual ~IMenuItem() = default;

    /** 在 whiptail 菜单中显示的用户可见标签（已翻译）。 */
    virtual std::string get_label() const = 0;

    /** 唯一标识符，用于 whiptail 选项路由和分发。 */
    virtual std::string get_tag() const = 0;

    /** 条件显示：返回 false 时此项不出现在当前菜单中。 */
    virtual bool is_visible(const MenuContext& ctx) const { (void)ctx; return true; }
};

} // namespace tmoe::ui
