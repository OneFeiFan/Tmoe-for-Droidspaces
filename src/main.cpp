#include "core/config.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "core/cli_parser.h"
#include "app/manager.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "core/executor.h"

/** 输出命令行使用帮助。 */
static void print_usage() {
    std::cout << R"(tmoes — GNU/Linux 容器管理工具 (现代化 C++17 版本)

用法:
  tmoes                       进入交互式 TUI 菜单
  tmoes [mode] [args...]      依据位置状态机解析引导容器运行

全局快捷命令选项:
  --lang <zh_CN|en_US>       手动覆盖国际化显示语言
  --quiet                    安静模式 (抑制 DEBUG 等输出)
  --no-color                 禁用彩色终端输出效果
  --help, -h                 查看此帮助
)";
}

/** 程序入口。
 *  阶段1: 解析全局标志 (--lang, --quiet, --no-color, --help)。
 *  阶段2: 初始化多语言子系统。
 *  阶段3: 自动检测运行环境，必要时提升权限。
 *  阶段4: 构建顶层应用管理器。
 *  阶段5: 路由至交互式 TUI 或命令行分发模式。
 */
int main(int argc, char *argv[]) {
    std::vector<std::string_view> pos_args;
    std::string lang = "zh_CN";

    // 阶段1: 剥离全局标志；剩余 token → 位置参数
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--lang" && i + 1 < argc) {
            lang = argv[++i];
        } else if (arg == "--quiet") {
            tmoe::Logger::quiet_mode = true;
        } else if (arg == "--no-color") {
            tmoe::Logger::enable_color = false;
        } else {
            pos_args.push_back(arg);
        }
    }

    // 阶段2: 加载 i18n 翻译
    tmoe::I18n::init(lang);

    // 阶段3: 自动检测环境并提权
    auto cfg = tmoe::TmoeConfig::detect();
    if (!cfg.is_termux && !cfg.is_root) {
        // escalate_privileges 调用 execvp —— 非特权进程映像被替换；
        // 此行之下的代码仅以 root 身份运行。
        tmoe::Executor::escalate_privileges(argc, argv);
    }
    cfg.ensure_dirs();

    // 阶段4: 初始化顶层应用管理器
    tmoe::app::Manager manager(std::move(cfg));

    // 阶段5: 路由至交互模式或命令分发
    if (pos_args.empty()) {
        return manager.run_interactive();
    }

    tmoe::LaunchContext ctx = tmoe::CliParser::parse(pos_args);

    // 复刻原版 Bash 行为：位置参数过多时发出警告
    if (pos_args.size() >= 7) {
        tmoe::Logger::error("ERROR, number of arguments exceeded.");
        std::fprintf(stderr, "Please retype the commands: tmoe %s %s %s %s %s %s\n",
                     pos_args[0].data(), pos_args[1].data(), pos_args[2].data(),
                     pos_args[3].data(), pos_args[4].data(), pos_args[5].data());
    }

    return manager.run_launch_context(ctx);
}
