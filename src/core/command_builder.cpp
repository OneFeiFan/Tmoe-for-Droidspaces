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

    CommandBuilder &CommandBuilder::add_flag(std::string flag) {
        args_.push_back(std::move(flag));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_flag_if(bool condition, std::string flag) {
        if (condition) args_.push_back(std::move(flag));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_kv(std::string key, std::string value) {
        args_.push_back(std::move(key) + "=" + std::move(value));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_kv_if(bool condition, std::string key, std::string value) {
        if (condition) args_.push_back(std::move(key) + "=" + std::move(value));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_opt(std::string opt, std::string value) {
        args_.push_back(std::move(opt));
        args_.push_back(std::move(value));
        return *this;
    }

    CommandBuilder &CommandBuilder::add_opt_if(bool condition, std::string opt, std::string value) {
        if (condition) {
            args_.push_back(std::move(opt));
            args_.push_back(std::move(value));
        }
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

    CommandBuilder &CommandBuilder::add_bind_to(std::string prefix, std::string source, std::string dest) {
        if (dest.empty()) {
            args_.push_back(std::move(prefix) + std::move(source));
        } else {
            args_.push_back(std::move(prefix) + std::move(source) + ":" + std::move(dest));
        }
        return *this;
    }

    CommandBuilder &CommandBuilder::add_bind_to_if(bool condition, std::string prefix,
                                                    std::string source, std::string dest) {
        if (condition) {
            add_bind_to(std::move(prefix), std::move(source), std::move(dest));
        }
        return *this;
    }

    CommandBuilder &CommandBuilder::add_env(std::string key, std::string value) {
        envs_.push_back(std::move(key) + "=" + std::move(value));
        return *this;
    }

    CommandBuilder &CommandBuilder::set_prefix(std::string prefix) {
        prefix_ = std::move(prefix);
        return *this;
    }

    CommandBuilder &CommandBuilder::add_raw(std::string text) {
        raw_parts_.push_back(std::move(text));
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
        if (!prefix_.empty()) {
            cmd += prefix_ + " ";
        }
        for (const auto &env: envs_) {
            cmd += env + " ";
        }
        cmd += bin_;
        for (const auto &arg: args_) {
            cmd += " " + shell_escape(arg);
        }
        for (const auto &raw: raw_parts_) {
            cmd += " " + raw;
        }
        return cmd;
    }

    ExecResult CommandBuilder::execute() const {
        std::string full_cmd = build_string();
        Logger::debug("Execute: " + full_cmd);
        return Executor::shell(full_cmd);
    }
} // namespace tmoe
