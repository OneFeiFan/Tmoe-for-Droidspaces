//
// Created by 30225 on 2026/5/25.
//

#ifndef RUNTIME_H
#define RUNTIME_H
#pragma once
#include <string>

#include "core/launch_context.h"

namespace tmoe::domain {
    class Container; // 前置声明

    /// 容器运行时的抽象接口 (策略模式)
    /// 负责具体的底层隔离环境启动与销毁逻辑
    class IContainerRuntime {
    public:
        virtual ~IContainerRuntime() = default;

        /// 生成启动命令（高内聚：各引擎自己负责自己的长参数拼接）
        virtual std::string generate_launch_cmd(const Container &container, const LaunchContext* ctx = nullptr) const = 0;

        /// 启动容器
        virtual bool start(const Container &container, const LaunchContext* ctx = nullptr) = 0;

        /// 停止容器
        virtual bool stop(const Container &container) = 0;
    };

    /// PRoot 运行时策略
    class ProotRuntime : public IContainerRuntime {
    public:
        std::string generate_launch_cmd(const Container &container, const LaunchContext* ctx = nullptr) const override;
        bool start(const Container &container, const LaunchContext* ctx = nullptr) override;
        bool stop(const Container &container) override;
    };

    // 未来拓展：class ChrootRuntime : public IContainerRuntime { ... };
} // namespace tmoe::domain
#endif //RUNTIME_H
