#include <catch2/catch_test_macros.hpp>
#include "core/executor.h"
#include "core/config.h"
#include "core/i18n.h"

using namespace tmoe;

TEST_CASE (
"Executor basic operations"
,
"[core]"
)
 {
    SECTION("echo returns correct output") {
        auto r = Executor::run("echo", {"hello", "world"});
        REQUIRE(r.ok());
        REQUIRE(r.stdout_data.find("hello world") != std::string::npos);
    }

    SECTION("nonexistent command fails") {
        auto r = Executor::run("nonexistent_cmd_12345", {});
        REQUIRE(!r.ok());
    }
}

TEST_CASE (
"Config detection"
,
"[core]"
)
 {
    auto cfg = TmoeConfig::detect();
    REQUIRE(!cfg.arch.empty());
    REQUIRE(!cfg.locale.empty());
}

TEST_CASE (
"I18n basic operations"
,
"[core]"
)
 {
    SECTION("default returns key") {
        REQUIRE(I18n::tr("nonexistent.key") == "nonexistent.key");
    }

    SECTION("custom registration") {
        I18n::register_custom("test", {
            {"hello", "world"}
        });
        REQUIRE(I18n::tr("hello") == "hello"); // 未切换到 test lang
    }
}

// 追加到 tests/test_core.cpp 的尾部
#include "core/cli_parser.h"

TEST_CASE (
"CliParser robust verification and boundaries"
,
"[core]"
)
 {
    using namespace tmoe;

    SECTION("Normal basic distribution code parser path") {
        std::vector<std::string_view> args = {"p", "ubuntu", "focal"};
        auto ctx = CliParser::parse(args);
        REQUIRE(ctx.mode == LaunchMode::Proot);
        REQUIRE(ctx.distro_name == "ubuntu");
        REQUIRE(ctx.distro_code == "focal");
        REQUIRE(ctx.temporary_script_file_01.empty());
    }

    SECTION("Smart absolute path evaluation from Git Bash env") {
        // 在 Windows Git Bash 下，用户输入的 /tmp/runner.sh 会自动被宿主重写为盘符大写路径形式
        std::vector<std::string_view> args = {"p", "debian", "sid", "C:/GitBash/tmp/runner.sh"};
        auto ctx = CliParser::parse(args);

        REQUIRE(ctx.mode == LaunchMode::Proot);
        REQUIRE(ctx.distro_name == "debian");
        // 应精准将其甄别为脚本文件而非普通的命令行字符串！
        REQUIRE(ctx.temporary_script_file_01 == "C:/GitBash/tmp/runner.sh");
        REQUIRE(ctx.exec_program_01.empty());
    }

    SECTION("Soft link state inversion triggers boundary rules") {
        std::vector<std::string_view> args = {"p", "arch", "rolling", "ln", "/etc/custom_dir"};
        auto ctx = CliParser::parse(args);

        REQUIRE(ctx.create_soft_link == true);
        // 状态发生成功反转：该路径被识别为了链接目录而不是临时脚本文件
        REQUIRE(ctx.link_source_dir == "/etc/custom_dir");
        REQUIRE(ctx.temporary_script_file_02.empty());
    }
}
