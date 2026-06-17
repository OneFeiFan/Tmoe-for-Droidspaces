#ifndef CLI_PARSER_H
#define CLI_PARSER_H
#pragma once
#include "core/launch_context.h"
#include <string_view>
#include <vector>
#include <filesystem>
#include <sstream>

namespace tmoe {

/** 将 CLI 位置参数解析为强类型 LaunchContext。
 *  实现了原版 Bash 脚本中的位置状态机逻辑。
 */
class CliParser {
public:
    /** 将过滤后的位置参数转换为 LaunchContext 对象。 */
    static LaunchContext parse(const std::vector<std::string_view>& pos_args);

private:
    /** 检测参数是否为文件系统路径（绝对或相对路径）。 */
    static bool is_path_argument(std::string_view arg);
};

} // namespace tmoe
#endif //CLI_PARSER_H
