#ifndef INSTALLABLE_APP_H
#define INSTALLABLE_APP_H
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/config.h"
#include "core/logger.h"
#include "core/i18n.h"
#include "core/executor.h"
#include "core/command_builder.hpp"
#include "core/system_helper.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

// ═══════════════════════════════════════════════════════════════
// DistroPkgNames — 按发行版家族映射包名
// ═══════════════════════════════════════════════════════════════

/** 各发行版家族的包名（空格分隔多包时自动拆分为数组）。
 *  common 字段作为未显式指定发行版的回退值。 */
struct DistroPkgNames {
    std::string debian;
    std::string arch;
    std::string redhat;
    std::string alpine;
    std::string gentoo;
    std::string suse;
    std::string common;     // 回退（优先级最低）

    /** 将空格分隔的包名拆分为字符串向量。 */
    static std::vector<std::string> split_pkgs(std::string_view s) {
        std::vector<std::string> result;
        if (s.empty()) return result;
        size_t start = 0;
        while (start < s.size()) {
            while (start < s.size() && s[start] == ' ') ++start;
            if (start >= s.size()) break;
            size_t end = s.find(' ', start);
            if (end == std::string_view::npos) end = s.size();
            result.emplace_back(s.substr(start, end - start));
            start = end;
        }
        return result;
    }

    /** 获取指定发行版家族对应的包名向量。 */
    std::vector<std::string> get(DistroFamily family) const {
        auto pkgs = [&]() -> std::string_view {
            switch (family) {
                case DistroFamily::Debian:  return debian;
                case DistroFamily::Arch:    return arch;
                case DistroFamily::RedHat:  return redhat;
                case DistroFamily::Alpine:  return alpine;
                case DistroFamily::Gentoo:  return gentoo;
                case DistroFamily::Suse:    return suse;
                default:                    return common;
            }
        }();
        if (pkgs.empty()) return split_pkgs(common);
        return split_pkgs(pkgs);
    }

    /** 检查某个发行版是否有显式配置（非空且非 common 回退）。 */
    bool has_explicit(DistroFamily family) const {
        auto pkgs = [&]() -> std::string_view {
            switch (family) {
                case DistroFamily::Debian:  return debian;
                case DistroFamily::Arch:    return arch;
                case DistroFamily::RedHat:  return redhat;
                case DistroFamily::Alpine:  return alpine;
                case DistroFamily::Gentoo:  return gentoo;
                case DistroFamily::Suse:    return suse;
                default:                    return {};
            }
        }();
        return !pkgs.empty();
    }
};

// ═══════════════════════════════════════════════════════════════
// InstallableApp — 可安装应用的抽象基类（模板方法模式）
// ═══════════════════════════════════════════════════════════════

/** 将每个"可安装的 Linux 应用"建模为对象。
 *
 *  子类只需覆盖 name() 和 packages() 两个纯虚函数，
 *  复杂的安装/卸载逻辑通过 hook 方法按需覆盖。
 *
 *  安装管线：
 *    confirm_install() → pre_install() → PackageManager::install() → post_install()
 *
 *  卸载管线：
 *    confirm_remove() → pre_remove() → PackageManager::remove() → post_remove()
 *
 *  使用示例：
 *    class ChromiumApp : public InstallableApp {
 *        std::string name() const override { return "Chromium"; }
 *        DistroPkgNames packages() const override {
 *            return {"chromium chromium-l10n chromium-driver", // debian
 *                    "chromium", "", "", "", "",                // arch
 *                    "chromium"};                               // common
 *        }
 *        bool pre_install(DistroFamily f) override {
 *            if (f == DistroFamily::Debian) ensure_chromium_ppa();
 *            return true;
 *        }
 *        bool post_install(DistroFamily f) override {
 *            create_sandbox_wrapper("Chromium", "chromium");
 *            return true;
 *        }
 *    };
 */
class InstallableApp {
public:
    explicit InstallableApp(const TmoeConfig& cfg) : cfg_(cfg) {}
    virtual ~InstallableApp() = default;

    // ── 标识（子类必须覆盖） ──

    /** 用户可见的应用名称（用于日志和 TUI 显示）。 */
    virtual std::string name() const = 0;

    /** 可执行文件名（用于 no-sandbox wrapper、desktop 图标名称）。
     *  默认与 name() 相同，Chromium/Edge 等需要覆盖。 */
    virtual std::string bin_name() const { return name(); }

    /** 应用描述（用于 TUI 提示，可选）。 */
    virtual std::string description() const { return ""; }

    /** 各发行版的包名配置。 */
    virtual DistroPkgNames packages() const = 0;

    // ── 安装管线钩子（子类按需覆盖） ──

    /** 安装前准备（添加 PPA、导入 GPG key、架构检查等）。
     *  返回 false 取消安装。 */
    virtual bool pre_install(DistroFamily /*family*/) { return true; }

    /** 安装后处理（创建 wrapper、配置环境变量、写 desktop 文件等）。
     *  仅在 PackageManager::install() 成功后才调用。 */
    virtual bool post_install(DistroFamily /*family*/) { return true; }

    /** 卸载前准备（解除 apt hold 等）。返回 false 取消卸载。 */
    virtual bool pre_remove(DistroFamily /*family*/) { return true; }

    /** 卸载后清理（删除 PPA、wrapper、desktop 文件等）。 */
    virtual bool post_remove(DistroFamily /*family*/) { return true; }

    // ── 特性标志 ──

    /** 是否需要创建 --no-sandbox 包装脚本（浏览器类应用专用）。 */
    virtual bool needs_sandbox_wrapper() const { return false; }

    // ── 核心操作（模板方法 — 一般不需要覆盖） ──

    /** 执行完整安装管线（子类可覆盖以自定义安装逻辑）。
     *  默认实现：
     *  1. pre_install(family)  →  false 则中止
     *  2. PackageManager::install(packages().get(family), family)
     *  3. post_install(family)  →  成功则调用
     *  4. needs_sandbox_wrapper()  →  自动创建 no-sandbox wrapper
     */
    virtual bool install(DistroFamily family);

    /** 执行完整卸载管线（子类可覆盖以自定义卸载逻辑）。
     *  默认实现：
     *  1. confirm_remove()      →  用户拒绝则中止
     *  2. pre_remove(family)    →  false 则中止
     *  3. PackageManager::remove(packages().get(family), family)
     *  4. post_remove(family)
     */
    virtual bool remove(DistroFamily family);

    // ── 用户交互 ──

    /** 显示安装确认 TUI 对话框。默认使用 whiptail --yesno。 */
    virtual bool confirm_install();

    /** 显示卸载确认 TUI 对话框。默认使用 whiptail --yesno。 */
    virtual bool confirm_remove();

protected:
    const TmoeConfig& cfg_;

    // ── 受保护的辅助方法（供子类 hook 使用） ──

    /** 创建 --no-sandbox 包装脚本 + .desktop 文件。
     *  适用于 Chromium 系浏览器等在沙箱环境中需要特权才能运行的 GUI 应用。
     *  @param display_name 桌面图标显示名称
     *  @param bin          可执行文件名（用于 $PATH 查找和 wrapper 命名）
     */
    static void create_sandbox_wrapper(std::string_view display_name, std::string_view bin);

    /** 创建 .desktop 文件（不创建 wrapper 脚本）。
     *  适用于只需要桌面快捷方式的应用。
     *  @param app_name  桌面图标显示名称
     *  @param exec_path 可执行文件路径（绝对路径）
     *  @param icon_name 图标名称（无扩展名）
     *  @param categories 桌面分类（分号分隔，如 "Network;WebBrowser;"）
     */
    static void create_desktop_entry(std::string_view app_name,
                                     std::string_view exec_path,
                                     std::string_view icon_name,
                                     std::string_view categories = "Utility;");
};

} // namespace tmoe::domain
#endif //INSTALLABLE_APP_H
