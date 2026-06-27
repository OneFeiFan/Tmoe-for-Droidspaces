#pragma once
#include <string>
#include <string_view>
#include <memory>
#include "core/config.h"
#include "../gui_config/registries.h"

namespace tmoe::domain {

// ── 通用数据结构 ──

/** 桌面会话命令对（对应 Xsession 脚本中的 SESSION_01/02） */
struct SessionCmds {
    std::string cmd1;
    std::string cmd2;
};

/** pre_install_choices() 返回值：包列表覆盖 */
struct PreInstallChoices {
    std::string pkg_list;           // 非空则覆盖 registry 的 pkg_group
    bool use_no_recommends = false; // apt --no-install-recommends
};

/** post_install_*() 上下文 */
struct PostInstallContext {
    DistroFamily family;
    bool is_debian;
    bool is_ubuntu;
    bool is_proot;
};

// ═══════════════════════════════════════════════════════════════
// 桌面基类
// ═══════════════════════════════════════════════════════════════

class DesktopBase {
public:
    explicit DesktopBase(const TmoeConfig& cfg) : cfg_(cfg) {}
    virtual ~DesktopBase() = default;

    // ── 标识 ──
    virtual std::string get_id() const = 0;
    virtual const DesktopInfo& get_info() const = 0;

    // ── 分类 ──
    virtual bool is_window_manager() const { return get_info().is_window_manager; }
    virtual bool needs_root() const { return get_info().requires_root; }

    // ── 会话命令（默认从 DesktopInfo 读取，GNOME/Budgie 可覆盖） ──
    virtual SessionCmds get_session_commands() const {
        return {get_info().session_cmd1, get_info().session_cmd2};
    }

    // ── 安装管线（子类按需覆盖） ──

    /** 阶段2: 装包前版本选择。返回空 pkg_list 表示不覆盖 registry */
    virtual PreInstallChoices pre_install_choices(
        DistroFamily family, bool is_auto_mode) {
        return {};
    }

    /** 阶段3: 系统配置（fonts, language packs, DE 专属设置） */
    virtual void post_install_config(const PostInstallContext& /*ctx*/) {}

    /** 阶段3b: 外观美化（wallpapers, themes, panels） */
    virtual void post_install_extras(const PostInstallContext& /*ctx*/) {}

    /** VNC 服务端推荐 */
    virtual bool recommends_tiger_vnc() const { return false; }

    /** 安装前提示 */
    virtual void will_be_installed_message() const {}

protected:
    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
