#ifndef COMMAND_BUILDER_HPP
#define COMMAND_BUILDER_HPP
#pragma once

#include "core/executor.h"
#include "logger.h"
#include <string>
#include <vector>

namespace tmoe {

/** 安全的命令构建器 (流式 API)。
 *  支持条件参数、多种挂载绑定风格、键值参数和环境变量注入。
 */
    class CommandBuilder {
    public:
        explicit CommandBuilder(std::string bin);

        /** 追加普通参数（自动 shell 转义）。 */
        CommandBuilder &add_arg(std::string arg);

        /** 按条件追加参数（消除零散的 if-else）。 */
        CommandBuilder &add_arg_if(bool condition, std::string arg);

        /** 追加布尔标志（如 --kill-on-exit、-0）。 */
        CommandBuilder &add_flag(std::string flag);

        /** 按条件追加布尔标志。 */
        CommandBuilder &add_flag_if(bool condition, std::string flag);

        /** 追加 --key=value 风格参数。 */
        CommandBuilder &add_kv(std::string key, std::string value);

        /** 按条件追加 --key=value 风格参数。 */
        CommandBuilder &add_kv_if(bool condition, std::string key, std::string value);

        /** 追加 --opt value 风格参数（选项 + 值分开）。 */
        CommandBuilder &add_opt(std::string opt, std::string value);

        /** 按条件追加 --opt value 风格参数。 */
        CommandBuilder &add_opt_if(bool condition, std::string opt, std::string value);

        /** 添加挂载绑定 (-b source:dest)。 */
        CommandBuilder &add_bind(std::string_view source, std::string_view dest = "");

        /** 按条件添加挂载绑定 (-b 风格)。 */
        CommandBuilder &add_bind_if(bool condition, std::string_view source, std::string_view dest = "");

        /** 通用挂载绑定：用自定义前缀连接 source:dest。
         *  prefix 示例："--bind="、"-v "、"-o bind "、" --bind " */
        CommandBuilder &add_bind_to(std::string prefix, std::string source, std::string dest);

        /** 按条件添加通用挂载绑定。 */
        CommandBuilder &add_bind_to_if(bool condition, std::string prefix,
                                       std::string source, std::string dest);

        /** 注入环境变量（KEY=VALUE 前置）。 */
        CommandBuilder &add_env(std::string key, std::string value);

        /** 设置命令前缀（如 sudo、su -c）。 */
        CommandBuilder &set_prefix(std::string prefix);

        /** 追加原始文本（不转义，用于管道、重定向等复杂场景）。
         *  注意：raw 内容不会经过 shell_escape，直接拼接到命令末尾。 */
        CommandBuilder &add_raw(std::string text);

        /** 构建完整转义后的命令字符串（用于调试）。 */
        std::string build_string() const;

        /** 构建并执行命令。 */
        ExecResult execute() const;

    private:
        std::string bin_;
        std::vector<std::string> args_;
        std::vector<std::string> envs_;
        std::string prefix_;
        std::vector<std::string> raw_parts_; // 不转义，直接拼在末尾
    };

} // namespace tmoe
#endif //COMMAND_BUILDER_HPP
