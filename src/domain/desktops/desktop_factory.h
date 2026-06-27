#pragma once
#include "desktop_base.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tmoe::domain {
    /** 桌面对象工厂。
     *  根据 ID 字符串创建对应的 DesktopBase 子类实例。
     *  已知 DE → 具体类；WM → WindowManagerDesktop（参数化）；未知 → 回退 xfce。 */
    class DesktopFactory {
    public:
        using CreatorFn = std::function<std::unique_ptr<DesktopBase>(const TmoeConfig &)>;

        /** 注册一个桌面类型。通常在静态初始化时调用。 */
        static void register_desktop(std::string id, CreatorFn creator);

        /** 创建桌面对象。ID 大小写不敏感。 */
        static std::unique_ptr<DesktopBase> create(
            std::string_view id, const TmoeConfig &cfg);

        /** 列出所有已注册的 ID */
        static std::vector<std::string> registered_ids();

    private:
        static std::unordered_map<std::string, CreatorFn> &registry();
    };
} // namespace tmoe::domain
