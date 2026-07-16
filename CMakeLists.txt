cmake_minimum_required(VERSION 3.12)
project(tmoes VERSION 1.0.0 LANGUAGES CXX)

# ═══════════════════════════════════════════════════════
# 依赖
# ═══════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ═══════════════════════════════════════════════════════
# 编译选项
# ═══════════════════════════════════════════════════════
option(TMOE_USE_LIBARCHIVE "Use libarchive for tar operations" OFF)
option(TMOE_USE_LIBCURL "Use libcurl for downloads" OFF)
option(TMOE_DEBUG "Enable debug logging (disable for release builds)" OFF)
option(TMOE_BUILD_TESTS "Build unit tests" OFF)

# ═══════════════════════════════════════════════════════
# 嵌入数据生成（i18n JSON 等 → C++ 头文件）
# ═══════════════════════════════════════════════════════
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        COMMAND ${CMAKE_COMMAND}
        -DLOCALES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/locales"
        -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.cmake
        COMMENT "Embedding i18n data..."
)

# ── JSON 数据嵌入 ──────────────────────────────────────
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/rootfs_map.json"
        -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h"
        -DVAR_NAME=ROOTFS_MAP_JSON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake
                ${CMAKE_CURRENT_SOURCE_DIR}/data/rootfs_map.json
        COMMENT "Embedding rootfs registry JSON..."
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/mirror_registry.json"
        -DOUTPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h"
        -DVAR_NAME=MIRROR_REGISTRY_JSON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake
                ${CMAKE_CURRENT_SOURCE_DIR}/data/mirror_registry.json
        COMMENT "Embedding mirror registry JSON..."
)

add_custom_target(gen_embedded_data DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
)

set(GENERATED_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
)

# ═══════════════════════════════════════════════════════
# 源码
# ═══════════════════════════════════════════════════════
file(GLOB_RECURSE TMOE_SOURCES
        src/core/*.cpp
        src/domain/**/*.cpp
        src/app/*.cpp
)

# ═══════════════════════════════════════════════════════
# 静态库（共享给可执行文件和测试）
# ═══════════════════════════════════════════════════════
add_library(tmoes_lib STATIC ${TMOE_SOURCES} ${GENERATED_HEADERS})

target_include_directories(tmoes_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/gen
)

add_dependencies(tmoes_lib gen_embedded_data)
target_link_libraries(tmoes_lib PUBLIC nlohmann_json::nlohmann_json)

# 平台宏（PUBLIC — 测试目标自动继承）
if (WIN32)
    target_compile_definitions(tmoes_lib PUBLIC TMOE_PLATFORM_WINDOWS)
elseif (APPLE)
    target_compile_definitions(tmoes_lib PUBLIC TMOE_PLATFORM_MACOS)
else ()
    target_compile_definitions(tmoes_lib PUBLIC TMOE_PLATFORM_LINUX)
endif ()

# 可选依赖
if (TMOE_USE_LIBARCHIVE)
    find_package(LibArchive REQUIRED)
    target_link_libraries(tmoes_lib PUBLIC LibArchive::LibArchive)
    target_compile_definitions(tmoes_lib PRIVATE TMOE_HAS_LIBARCHIVE)
endif ()

if (TMOE_USE_LIBCURL)
    find_package(CURL REQUIRED)
    target_link_libraries(tmoes_lib PUBLIC CURL::libcurl)
    target_compile_definitions(tmoes_lib PRIVATE TMOE_HAS_LIBCURL)
endif ()

if (TMOE_DEBUG)
    target_compile_definitions(tmoes_lib PUBLIC TMOE_DEBUG)
endif ()

# ═══════════════════════════════════════════════════════
# 可执行文件
# ═══════════════════════════════════════════════════════
add_executable(tmoes src/main.cpp)
target_link_libraries(tmoes PRIVATE tmoes_lib)
add_dependencies(tmoes gen_embedded_data)

# ═══════════════════════════════════════════════════════
# UI 插件化框架（添加到静态库）
# ═══════════════════════════════════════════════════════
add_subdirectory(src/ui)

# ═══════════════════════════════════════════════════════
# 单元测试
# ═══════════════════════════════════════════════════════
if (TMOE_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    file(GLOB TEST_SOURCES tests/*.cpp tests/core/*.cpp)
    add_executable(tmoe_tests ${TEST_SOURCES})
    target_link_libraries(tmoe_tests PRIVATE tmoes_lib doctest)
    # includes 和平台宏从 tmoes_lib PUBLIC 自动继承

    add_test(NAME tmoe_unit COMMAND tmoe_tests)
endif ()
