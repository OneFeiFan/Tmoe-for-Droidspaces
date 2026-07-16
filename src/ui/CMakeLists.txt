# UI framework sources
target_sources(tmoes_lib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menu_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/registry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin.h
        # ── 插件 .cpp ──
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/main_menu.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/backup_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/beta_features_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/browser_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/config_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/dev_tools_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/docker_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/download_tools_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/education_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_beautify_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_desktop_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_remote_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/input_method_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/office_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/software_center_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/terminal_app_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/termux_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/virtualization_plugin.cpp
        # ── 插件 .h（IDE 可见性）──
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/backup_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/beta_features_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/browser_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/config_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/dev_tools_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/docker_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/download_tools_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/education_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_beautify_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_desktop_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_remote_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/input_method_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/office_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/software_center_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/terminal_app_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/termux_plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/menus/virtualization_plugin.h
)