//
// Created by 30225 on 2026/5/25.
//

#ifndef CLI_PARSER_H
#define CLI_PARSER_H
#pragma once
#include "core/launch_context.h"
#include <string_view>
#include <vector>

namespace tmoe {

    class CliParser {
    public:
        /// 将过滤掉全局 Flag 后的纯位置参数，转换为高内聚的 LaunchContext 上下文对象
        static LaunchContext parse(const std::vector<std::string_view>& pos_args);

    private:
        /// 智能识别是否为“路径文件”参数，完美适配 Git Bash 自动转换后的 Windows 绝对路径
        static bool is_path_argument(std::string_view arg);
    };

} // namespace tmoe
#endif //CLI_PARSER_H
