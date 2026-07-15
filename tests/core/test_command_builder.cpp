/** CommandBuilder::build_string 单元测试 */
#include <doctest/doctest.h>
#include "core/command_builder.hpp"

using tmoe::CommandBuilder;

TEST_CASE("CommandBuilder: basic construction") {
    // 最简命令
    CHECK(CommandBuilder("ls").build_string() == "ls");
    CHECK(CommandBuilder("echo").add_arg("hello").build_string() == "echo 'hello'");
}

TEST_CASE("CommandBuilder: flags") {
    // 布尔标志和条件标志
    CHECK(CommandBuilder("apt").add_flag("install").add_flag("-y")
              .build_string() == "apt 'install' '-y'");

    CHECK(CommandBuilder("cmd").add_flag_if(true, "-v")
              .build_string() == "cmd '-v'");
    CHECK(CommandBuilder("cmd").add_flag_if(false, "-v")
              .build_string() == "cmd");
}

TEST_CASE("CommandBuilder: key-value pairs") {
    // add_kv 生成 --key=value
    CHECK(CommandBuilder("proot").add_kv("--kill-on-exit", "")
              .build_string() == "proot '--kill-on-exit='");
    CHECK(CommandBuilder("proot").add_kv("--rootfs", "/data")
              .build_string() == "proot '--rootfs=/data'");
}

TEST_CASE("CommandBuilder: option + value pairs") {
    // add_opt 生成两个独立参数
    auto cmd = CommandBuilder("gcc").add_opt("-o", "out.o").build_string();
    CHECK(cmd == "gcc '-o' 'out.o'");
}

TEST_CASE("CommandBuilder: conditional args") {
    // add_arg_if: 条件为真才添加
    CHECK(CommandBuilder("apt").add_arg_if(true, "install")
              .add_arg_if(false, "remove")
              .build_string() == "apt 'install'");
}

TEST_CASE("CommandBuilder: prefix") {
    // set_prefix 在命令前添加前缀
    CHECK(CommandBuilder("apt").set_prefix("sudo")
              .add_arg("install").add_arg("firefox")
              .build_string() == "sudo apt 'install' 'firefox'");
}

TEST_CASE("CommandBuilder: raw parts") {
    // add_raw 不转义
    CHECK(CommandBuilder("apt").add_arg("update")
              .add_raw("2>/dev/null").build_string()
          == "apt 'update' 2>/dev/null");
}

TEST_CASE("CommandBuilder: environment variables") {
    // add_env 在命令前添加 KEY=VALUE
    auto cmd = CommandBuilder("make")
                   .add_env("CC", "gcc")
                   .add_env("CFLAGS", "-O2")
                   .build_string();
    // env value 经过转义
    CHECK(cmd.find("CC='gcc'") != std::string::npos);
    CHECK(cmd.find("CFLAGS='-O2'") != std::string::npos);
    CHECK(cmd.find(" make") != std::string::npos);
}

TEST_CASE("CommandBuilder: multiple args") {
    // 混合使用
    auto cmd = CommandBuilder("tar")
                   .add_flag("-xzf")
                   .add_arg("archive.tar.gz")
                   .add_opt("-C", "/tmp")
                   .build_string();
    CHECK(cmd == "tar '-xzf' 'archive.tar.gz' '-C' '/tmp'");
}
