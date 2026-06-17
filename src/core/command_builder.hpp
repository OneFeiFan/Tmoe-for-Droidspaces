#ifndef COMMAND_BUILDER_HPP
#define COMMAND_BUILDER_HPP
#pragma once
#include "core/executor.h"
#include "logger.h"
#include <string>
#include <vector>

namespace tmoe {

/** 安全的命令构建器 (流式 API)。
 *  支持条件参数、挂载绑定和环境变量注入。
 */
class CommandBuilder {
public:
    explicit CommandBuilder(std::string bin);

    /** 追加普通参数。 */
    CommandBuilder &add_arg(std::string arg);

    /** 按条件追加参数（消除零散的 if-else）。 */
    CommandBuilder &add_arg_if(bool condition, std::string arg);

    /** 添加挂载绑定 (-b source:dest)。 */
    CommandBuilder &add_bind(std::string_view source, std::string_view dest = "");

    /** 按条件添加挂载绑定。 */
    CommandBuilder &add_bind_if(bool condition, std::string_view source, std::string_view dest = "");

    /** 注入环境变量。 */
    CommandBuilder &add_env(std::string key, std::string value);

    /** 构建完整转义后的命令字符串（用于调试）。 */
    std::string build_string() const;

    /** 构建并执行命令。 */
    ExecResult execute() const;

private:
    std::string bin_;
    std::vector<std::string> args_;
    std::vector<std::string> envs_;

    static std::string shell_escape(std::string_view arg);
};

} // namespace tmoe
#endif //COMMAND_BUILDER_HPP
