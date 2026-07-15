/** shell_escape 函数单元测试 */
#include <doctest/doctest.h>
#include "core/executor.h"

using tmoe::shell_escape;

TEST_CASE("shell_escape: normal strings") {
    // 普通字符串被单引号包裹
    CHECK(shell_escape("hello") == "'hello'");
    CHECK(shell_escape("hello world") == "'hello world'");
    CHECK(shell_escape("abc123") == "'abc123'");
}

TEST_CASE("shell_escape: empty string") {
    // 空字符串返回空单引号对
    CHECK(shell_escape("") == "''");
}

TEST_CASE("shell_escape: single quote inside") {
    // 单引号被替换为 '\''
    CHECK(shell_escape("it's") == "'it'\\''s'");
    CHECK(shell_escape("'quoted'") == "''\\''quoted'\\'''");
}

TEST_CASE("shell_escape: special characters") {
    // 特殊字符原样保留，因为单引号内一切字面
    CHECK(shell_escape("$HOME") == "'$HOME'");
    CHECK(shell_escape("a; rm -rf /") == "'a; rm -rf /'");
    CHECK(shell_escape("`id`") == "'`id`'");
    CHECK(shell_escape("$(whoami)") == "'$(whoami)'");
}
