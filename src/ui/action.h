#pragma once
#include "menu_item.h"

namespace tmoe::ui {

/**
 * 叶子操作基类。
 *
 * 表示菜单树的终端节点——用户选择后直接执行操作，
 * 不进入下级菜单。
 */
class IAction : public IMenuItem {
public:
    /** 执行操作。返回 true 表示成功。 */
    virtual bool execute(MenuContext& ctx) = 0;

    /** 是否需要 root 权限（入口处统一提权检查）。 */
    virtual bool needs_root() const { return true; }
};

} // namespace tmoe::ui
