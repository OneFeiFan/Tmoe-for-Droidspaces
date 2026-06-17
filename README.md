# tmoe-cpp

> tmoe-linux 的 C++17 现代化重写。
> Bash 胶水 → 类型安全 + 模块隔离 + i18n。

## 构建

```bash
mkdir build && cd build
cmake .. -DTMOE_EMBED_I18N=ON
make -j$(nproc)

# 可选增强
cmake .. -DTMOE_USE_LIBARCHIVE=ON -DTMOE_USE_LIBCURL=ON
```

无硬依赖。纯 C++17 + STL 即可编译，通过 `popen`/`fork+exec` 调用系统命令。

## 架构

```
tmoe
├── core/        # 基础设施 (Executor / Config / Logger / I18n)
├── domain/      # 领域逻辑 (Container / GUI / Mirrors / Backup ...)
├── app/         # 应用编排 (Manager / SoftwareCenter)
├── locales/     # 多语言 JSON
└── tests/       # 单元测试
```

## i18n

添加新语言只需在 `locales/` 下放一个 JSON：

```json
{ "key": "translation" }
```

代码中用 `I18n::tr("key")` 即可。编译时 `TMOE_EMBED_I18N=ON` 会把 JSON 嵌入二进制。

## 许可证

与原 tmoe-linux 保持一致。
