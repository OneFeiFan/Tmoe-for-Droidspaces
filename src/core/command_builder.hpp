//
// Created by 30225 on 2026/5/25.
//

#ifndef COMMAND_BUILDER_HPP
#define COMMAND_BUILDER_HPP
#pragma once
#include "core/executor.h"
#include <string>
#include <vector>

namespace tmoe {
    /// 安全的终端命令建造者模式 (Fluent API)
    class CommandBuilder {
    public:
        explicit CommandBuilder(std::string bin);

        /// 追加普通参数
        CommandBuilder &add_arg(std::string arg);

        /// 根据条件追加参数（完美消除零碎的 if-else）
        CommandBuilder &add_arg_if(bool condition, std::string arg);

        /// 添加挂载点 (等价于 -b source:dest)
        CommandBuilder &add_bind(std::string_view source, std::string_view dest = "");

        /// 根据条件添加挂载点
        CommandBuilder &add_bind_if(bool condition, std::string_view source, std::string_view dest = "");

        /// 注入环境变量
        CommandBuilder &add_env(std::string key, std::string value);

        /// 输出用于调试的完整安全命令字符串
        std::string build_string() const;

        /// 执行最终拼装的命令
        ExecResult execute() const;

    private:
        std::string bin_;
        std::vector<std::string> args_;
        std::vector<std::string> envs_;

        // 内部转义逻辑
        static std::string shell_escape(std::string_view arg);
    };
} // namespace tmoe
#endif //COMMAND_BUILDER_HPP
