#include "core/config.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "core/cli_parser.h"
#include "app/manager.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/executor.h"

/** 输出命令行使用帮助。 */
static void print_usage() {
    std::cout << _("cli.usage");
}

/** 程序入口。
 *  阶段1: 解析全局标志 (--lang, --quiet, --no-color, --help)。
 *  阶段2: 初始化多语言子系统。
 *  阶段3: 自动检测运行环境，必要时提升权限。
 *  阶段4: 构建顶层应用管理器。
 *  阶段5: 路由至交互式 TUI 或命令行分发模式。
 */
/** 读取持久化的 locale 偏好 (若存在)。 */
static std::string load_saved_locale() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    std::string path = std::string(home) + "/.config/tmoe-linux/locale";
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string lang;
    std::getline(f, lang);
    // 清理换行符
    while (!lang.empty() && (lang.back() == '\n' || lang.back() == '\r'))
        lang.pop_back();
    return lang;
}

/** 持久化 locale 偏好到文件。 */
static void save_locale_pref(std::string_view lang) {
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string dir  = std::string(home) + "/.config/tmoe-linux";
    std::string path = dir + "/locale";
    fs::create_directories(dir);
    std::ofstream f(path);
    if (f.is_open()) f << lang << "\n";
}

int main(int argc, char *argv[]) {
    std::vector<std::string_view> pos_args;
    std::string lang = "en_US";  // 默认英文
    bool lang_from_cli = false;

    // 阶段1: 剥离全局标志；剩余 token → 位置参数
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--lang" && i + 1 < argc) {
            lang = argv[++i];
            lang_from_cli = true;
        } else if (arg == "--quiet") {
            tmoe::Logger::quiet_mode = true;
        } else if (arg == "--no-color") {
            tmoe::Logger::enable_color = false;
        } else {
            pos_args.push_back(arg);
        }
    }

    // 阶段2: 加载 i18n 翻译 (先用默认/CLI 值)
    tmoe::I18n::init(lang);

    // 阶段3: 自动检测环境并提权
    auto cfg = tmoe::TmoeConfig::detect();
    if (!cfg.is_termux && !cfg.is_root) {
        // escalate_privileges 调用 execvp —— 非特权进程映像被替换；
        // 此行之下的代码仅以 root 身份运行。
        tmoe::Executor::escalate_privileges(argc, argv);
    }
    cfg.ensure_dirs();

    // 阶段3.5: 读取持久化的 locale 偏好 (优先级: CLI > 持久化 > 默认英文)
    if (!lang_from_cli) {
        std::string saved = load_saved_locale();
        if (!saved.empty() && saved != lang) {
            lang = saved;
            tmoe::I18n::init(lang);
        }
    }

    // 阶段4: 初始化顶层应用管理器 (传入 save_locale_pref 回调)
    tmoe::app::Manager manager(std::move(cfg), save_locale_pref);

    // 阶段5: 路由至交互模式或命令分发
    if (pos_args.empty()) {
        return manager.run_interactive();
    }

    tmoe::LaunchContext ctx = tmoe::CliParser::parse(pos_args);

    // 复刻原版 Bash 行为：位置参数过多时发出警告
    if (pos_args.size() >= 7) {
        tmoe::Logger::error(_("cli.too_many_args"));
        std::fprintf(stderr, "%s tmoe %s %s %s %s %s %s\n",
                     _("cli.retype_hint").c_str(),
                     pos_args[0].data(), pos_args[1].data(), pos_args[2].data(),
                     pos_args[3].data(), pos_args[4].data(), pos_args[5].data());
    }

    return manager.run_launch_context(ctx);
}
