cmake_minimum_required(VERSION 3.16)
project(tmoe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── 外部依赖 (自动拉取) ────────────────────────
include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── 可选依赖 ──────────────────────────────────
# 核心: 无硬依赖（纯 popen/system 调用外部命令即可工作）
# 增强: libarchive（tar 原生操作）、libcurl（HTTP 下载）
#        libintl（gettext i18n，也可不用）

option(TMOE_USE_LIBARCHIVE "Use libarchive for tar operations" OFF)
option(TMOE_USE_LIBCURL "Use libcurl for downloads" OFF)

# ── i18n & 数据驱动嵌入 ────────────────────────

file(GLOB LOCALE_JSON_FILES "${CMAKE_CURRENT_SOURCE_DIR}/locales/*.json")
# 用 Python 脚本将 JSON 转为 C++ 头文件
find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
    # 1. 生成 i18n 语言包数据
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
            COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.py
            ${CMAKE_CURRENT_SOURCE_DIR}/locales
            ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
            DEPENDS tools/embed_i18n.py ${LOCALE_JSON_FILES}
            COMMENT "Embedding locale data"
    )

    # 2. 生成 rootfs 容器镜像映射数据
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
            COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.py
            ${CMAKE_CURRENT_SOURCE_DIR}/data/rootfs_map.json
            ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
            "ROOTFS_MAP_JSON" # 传入生成变量名
            DEPENDS tools/embed_json.py ${CMAKE_CURRENT_SOURCE_DIR}/data/rootfs_map.json
            COMMENT "Embedding rootfs map data"
    )

    # 3. 生成镜像源注册表数据
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
            COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.py
            ${CMAKE_CURRENT_SOURCE_DIR}/data/mirror_registry.json
            ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
            "MIRROR_REGISTRY_JSON"
            DEPENDS tools/embed_json.py ${CMAKE_CURRENT_SOURCE_DIR}/data/mirror_registry.json
            COMMENT "Embedding mirror registry data"
    )
endif ()


# ── 源文件 ─────────────────────────────────────
file(GLOB_RECURSE TMOE_SOURCES
        src/main.cpp
        src/core/*.cpp
        src/domain/*.cpp
        src/domain/*/*.cpp
        src/app/*.cpp
)

add_executable(tmoes ${TMOE_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
)

target_include_directories(tmoes PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/gen  # <-- 这个设置非常棒，编译期文件和源码分离了
)

# 链接 nlohmann_json
target_link_libraries(tmoes PRIVATE nlohmann_json::nlohmann_json)


# 平台相关
if (WIN32)
    target_compile_definitions(tmoes PRIVATE TMOE_PLATFORM_WINDOWS)
elseif (APPLE)
    target_compile_definitions(tmoes PRIVATE TMOE_PLATFORM_MACOS)
else ()
    target_compile_definitions(tmoes PRIVATE TMOE_PLATFORM_LINUX)
endif ()


# ── 可选依赖链接 ───────────────────────────────
if (TMOE_USE_LIBARCHIVE)
    find_package(LibArchive REQUIRED)
    target_link_libraries(tmoes PRIVATE LibArchive::LibArchive)
    target_compile_definitions(tmoes PRIVATE TMOE_HAS_LIBARCHIVE)
endif ()

if (TMOE_USE_LIBCURL)
    find_package(CURL REQUIRED)
    target_link_libraries(tmoes PRIVATE CURL::libcurl)
    target_compile_definitions(tmoes PRIVATE TMOE_HAS_LIBCURL)
endif ()

