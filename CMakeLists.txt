cmake_minimum_required(VERSION 3.12)
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
option(TMOE_DEBUG "Enable debug logging (disable for release builds)" OFF)

# ── i18n & 数据驱动嵌入 (纯 CMake 零依赖架构) ────
# 先确保输出目录存在
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen)

# 在 locales 目录下新增或删除了 .json 文件，CMake 会自动触发重新初始化
file(GLOB LOCALE_JSON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/locales/*.json")
set(ROOTFS_MAP "${CMAKE_CURRENT_SOURCE_DIR}/data/rootfs_map.json")
set(MIRROR_REG_MAP "${CMAKE_CURRENT_SOURCE_DIR}/data/mirror_registry.json")


# 利用 execute_process 在执行 cmake .. 初始化时立刻运行脚本。
message(STATUS "Pre-generating embedded data headers for IDE indexing...")

execute_process(
        COMMAND ${CMAKE_COMMAND}
        -DLOCALES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/locales
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.cmake"
)
execute_process(
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${ROOTFS_MAP}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        -DVAR_NAME=ROOTFS_MAP_JSON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake"
)
execute_process(
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${MIRROR_REG_MAP}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
        -DVAR_NAME=MIRROR_REGISTRY_JSON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake"
)

# 利用 add_custom_command 与 DEPENDS 将脚本与源文件绑定。
# 修改了某个 json 里面的文字，点击 Build 或 Make 时，能自动触发增量重新生成。
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        COMMAND ${CMAKE_COMMAND}
        -DLOCALES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/locales
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_i18n.cmake" ${LOCALE_JSON_FILES}
        COMMENT "Re-embedding locale data..."
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${ROOTFS_MAP}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        -DVAR_NAME=ROOTFS_MAP_JSON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake" "${ROOTFS_MAP}"
        COMMENT "Re-embedding rootfs map data..."
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
        COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${MIRROR_REG_MAP}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
        -DVAR_NAME=MIRROR_REGISTRY_JSON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_json.cmake" "${MIRROR_REG_MAP}"
        COMMENT "Re-embedding mirror registry data..."
)

# 统一管理生成的头文件
set(GENERATED_HEADERS
        ${CMAKE_CURRENT_BINARY_DIR}/gen/i18n_data.h
        ${CMAKE_CURRENT_BINARY_DIR}/gen/rootfs_data.h
        ${CMAKE_CURRENT_BINARY_DIR}/gen/mirror_data.h
)

# 自定义目标：把三个生成文件打包成一个构建步骤
add_custom_target(gen_embedded_data DEPENDS ${GENERATED_HEADERS})


# ── 源文件 ─────────────────────────────────────
file(GLOB_RECURSE TMOE_SOURCES
        src/main.cpp
        src/core/*.cpp
        src/domain/*.cpp
        src/domain/*/*.cpp
        src/app/*.cpp
)

add_executable(tmoes ${TMOE_SOURCES} ${GENERATED_HEADERS})

target_include_directories(tmoes PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/gen
)

# 确保编译可执行文件前，必定先执行嵌入数据的生成
add_dependencies(tmoes gen_embedded_data)

# 链接 nlohmann_json
target_link_libraries(tmoes PRIVATE nlohmann_json::nlohmann_json)

# 平台相关宏定义
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

# ── 调试日志编译期控制 ─────────────────────────
if (TMOE_DEBUG)
    target_compile_definitions(tmoes PRIVATE TMOE_DEBUG)
endif ()

# ── UI 插件化框架 ──────────────────────────────
add_subdirectory(src/ui)

# ── 单元测试 ─────────────────────────────────────
option(TMOE_BUILD_TESTS "Build unit tests" OFF)

if (TMOE_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    file(GLOB TEST_SOURCES tests/*.cpp tests/core/*.cpp)
    add_executable(tmoe_tests ${TEST_SOURCES})
    target_link_libraries(tmoe_tests PRIVATE doctest)
    target_include_directories(tmoe_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/gen
    )
    target_compile_definitions(tmoe_tests PRIVATE TMOE_PLATFORM_LINUX)

    add_test(NAME tmoe_unit COMMAND tmoe_tests)
endif ()