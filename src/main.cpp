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

int main(int argc, char *argv[]) {
    std::vector<std::string_view> pos_args;
    std::string lang = "zh_CN";

    // ── 1. 高内聚解析：优先剥离处理全局核心 Flags ──
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
            // 剩下的全部归纳为高内聚的位置参数序列
            pos_args.push_back(arg);
        }
    }

    // ── 2. 多语言基础设施热加载 ──
    tmoe::I18n::init(lang);

    // ── 3. 环境与配置全自动探针检测 ──
    auto cfg = tmoe::TmoeConfig::detect();
    if (!cfg.is_termux && !cfg.is_root) {
        // 提权成功后，操作系统会用 root 权限重新执行 tmoes 程序
        // 原有的非特权进程直接被覆盖，代码永远不会走到下一行
        tmoe::Executor::escalate_privileges(argc, argv);
    }
    cfg.ensure_dirs();

    // ── 4. 初始化顶层编排管理器 ──
    tmoe::app::Manager manager(std::move(cfg));

    // ── 5. 核心调度路由：判断进入交互模式还是命令分发模式 ──
    if (pos_args.empty()) {
        return manager.run_interactive();
    } else {
        // 执行位置状态机深度解析转换
        tmoe::LaunchContext ctx = tmoe::CliParser::parse(pos_args);

        // 健壮性检验：还原原 Bash 超出参数上限的异常警告
        if (pos_args.size() >= 7) {
            tmoe::Logger::error("ERROR, number of arguments exceeded.");
            std::fprintf(stderr, "Please retype the commands: tmoe %s %s %s %s %s %s\n",
                         pos_args[0].data(), pos_args[1].data(), pos_args[2].data(),
                         pos_args[3].data(), pos_args[4].data(), pos_args[5].data());
        }

        // 把完整的强类型上下文安全的投递给处理模块
        return manager.run_launch_context(ctx);
    }
}
