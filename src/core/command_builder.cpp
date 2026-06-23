#include "command_builder.hpp"

namespace tmoe {
    CommandBuilder::CommandBuilder(std::string bin) : bin_(std::move(bin)) {
    }

    CommandBuilder &CommandBuilder::add_arg(std::string arg) {
        args_.push_back(std::move(arg));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_arg_if(bool condition, std::string arg) {
        if (condition) args_.push_back(std::move(arg));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_bind(std::string_view source, std::string_view dest) {
        if (dest.empty()) {
            args_.emplace_back("-b");
            args_.emplace_back(source);
        } else {
            args_.emplace_back("-b");
            args_.push_back(std::string(source) + ":" + std::string(dest));
        }
        return *this;
    }

    CommandBuilder &CommandBuilder::add_bind_if(bool condition, std::string_view source, std::string_view dest) {
        if (condition) {
            add_bind(source, dest);
        }
        return *this;
    }

    CommandBuilder &CommandBuilder::add_env(std::string key, std::string value) {
        envs_.push_back(std::move(key) + "=" + std::move(value));
        return *this;
    }

    std::string CommandBuilder::shell_escape(std::string_view arg) {
        std::string escaped = "'";
        for (char c: arg) {
            if (c == '\'') escaped += "'\\''";
            else escaped += c;
        }
        escaped += "'";
        return escaped;
    }

    std::string CommandBuilder::build_string() const {
        std::string cmd;
        for (const auto &env: envs_) {
            cmd += env + " ";
        }
        cmd += bin_;
        for (const auto &arg: args_) {
            cmd += " " + shell_escape(arg);
        }
        return cmd;
    }

    ExecResult CommandBuilder::execute() const {
        std::string full_cmd = build_string();
        Logger::debug("Execute: " + full_cmd);
        return Executor::shell(full_cmd);
    }
} // namespace tmoe
