#pragma once
#include "menu.h"
#include <memory>

namespace tmoe::ui {

/**
 * UI 插件抽象基类。
 *
 * 每个领域模块对应一个插件类（如 DesktopMenuPlugin、BrowserMenuPlugin），
 * 负责构建该模块的 IUIMenu 菜单树。
 *
 * 使用方式：
 *   MenuEngine(ctx).run(DesktopMenuPlugin(gui).build());
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /** 构建并返回完整的菜单树（含导航项）。 */
    virtual std::shared_ptr<IUIMenu> build() = 0;
};

/**
 * 通用插件适配器——消除各插件类重复的构造函数/mgr_ 样板。
 *
 * 插件类继承此类而非直接继承 IPlugin，基类自动处理 Manager 指针存储：
 *   class BrowserMenuPlugin : public PluginFor<domain::BrowserManager> {
 *   public:
 *       using PluginFor::PluginFor;  // 继承构造函数
 *       std::shared_ptr<IUIMenu> build() override;
 *   };
 */
template<typename Mgr>
class PluginFor : public IPlugin {
public:
    explicit PluginFor(Mgr* mgr) : mgr_(mgr) {}
protected:
    Mgr* mgr_;
};

} // namespace tmoe::ui
