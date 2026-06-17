#ifndef RUNTIME_H
#define RUNTIME_H
#pragma once
#include <string>
#include "core/launch_context.h"

namespace tmoe::domain {
    class Container; // 前置声明

    /** 容器运行时抽象接口（策略模式）。
     *  各引擎自行处理隔离环境的搭建与销毁。
     */
    class IContainerRuntime {
    public:
        virtual ~IContainerRuntime() = default;

        /** 为指定容器生成启动命令字符串。 */
        virtual std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const =
        0;

        /** 启动容器。 */
        virtual bool start(const Container &container, const LaunchContext *ctx = nullptr) = 0;

        /** 停止容器。 */
        virtual bool stop(const Container &container) = 0;
    };

    /** PRoot 运行时策略。 */
    class ProotRuntime : public IContainerRuntime {
    public:
        std::string generate_launch_cmd(const Container &container, const LaunchContext *ctx = nullptr) const override;

        bool start(const Container &container, const LaunchContext *ctx = nullptr) override;

        bool stop(const Container &container) override;
    };

    // 待办: class ChrootRuntime : public IContainerRuntime { ... };
    // 待办: class NspawnRuntime : public IContainerRuntime { ... };
} // namespace tmoe::domain
#endif //RUNTIME_H
