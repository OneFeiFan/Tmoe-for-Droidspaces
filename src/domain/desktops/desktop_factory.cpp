#include "desktop_factory.h"
#include "window_manager_desktop.h"
#include "xfce_desktop.h"
#include "xfce_lite_desktop.h"
#include "kde_desktop.h"
#include "mate_desktop.h"
#include "lxde_desktop.h"
#include "lxqt_desktop.h"
#include "cinnamon_desktop.h"
#include "gnome_desktop.h"
#include "budgie_desktop.h"
#include "dde_desktop.h"
#include "deepin_desktop.h"
#include "ukui_desktop.h"
#include "cutefish_desktop.h"
#include "../gui_config/registries.h"
#include "core/logger.h"
#include <algorithm>

namespace {
    // 静态初始化：注册所有已知桌面类
#define REGISTER_DESKTOP(id, Class) \
    DesktopFactory::register_desktop(id, \
        [](const tmoe::TmoeConfig& c) { return std::make_unique<Class>(c); })

    struct AutoRegister {
        AutoRegister() {
            using namespace tmoe::domain;
            REGISTER_DESKTOP("xfce", XfceDesktop);
            REGISTER_DESKTOP("xfce-lite", XfceLiteDesktop);
            REGISTER_DESKTOP("kde", KdeDesktop);
            REGISTER_DESKTOP("mate", MateDesktop);
            REGISTER_DESKTOP("lxde", LxdeDesktop);
            REGISTER_DESKTOP("lxqt", LxqtDesktop);
            REGISTER_DESKTOP("cinnamon", CinnamonDesktop);
            REGISTER_DESKTOP("gnome", GnomeDesktop);
            REGISTER_DESKTOP("budgie", BudgieDesktop);
            REGISTER_DESKTOP("dde", DdeDesktop);
            REGISTER_DESKTOP("deepin", DeepinDesktop);
            REGISTER_DESKTOP("ukui", UkuiDesktop);
            REGISTER_DESKTOP("cutefish", CutefishDesktop);
        }
    } auto_register;

#undef REGISTER_DESKTOP
}

namespace tmoe::domain {
    std::unordered_map<std::string, DesktopFactory::CreatorFn> &
    DesktopFactory::registry() {
        static std::unordered_map<std::string, CreatorFn> reg;
        return reg;
    }

    void DesktopFactory::register_desktop(std::string id, CreatorFn creator) {
        std::transform(id.begin(), id.end(), id.begin(), ::tolower);
        registry()[std::move(id)] = std::move(creator);
    }

    std::unique_ptr<DesktopBase> DesktopFactory::create(
            std::string_view id, const TmoeConfig &cfg) {
        std::string id_lower(id);
        std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

        // 1. 查找已注册的桌面类
        auto &reg = registry();
        auto it = reg.find(id_lower);
        if (it != reg.end()) {
            return it->second(cfg);
        }

        // 2. 窗口管理器回退
        for (const auto &info: gui_config::all_desktops()) {
            std::string info_id(info.id);
            std::transform(info_id.begin(), info_id.end(), info_id.begin(), ::tolower);
            if (info_id == id_lower && info.is_window_manager) {
                return std::make_unique<WindowManagerDesktop>(cfg, info);
            }
        }

        // 3. 完全未知 → 警告 + 回退到 xfce
        Logger::warn("Unknown desktop id '" + std::string(id) + "', falling back to xfce");
        it = reg.find("xfce");
        if (it != reg.end()) return it->second(cfg);
        return nullptr;
    }

    std::vector<std::string> DesktopFactory::registered_ids() {
        std::vector<std::string> ids;
        for (const auto &[id, _]: registry()) ids.push_back(id);
        return ids;
    }
} // namespace tmoe::domain
