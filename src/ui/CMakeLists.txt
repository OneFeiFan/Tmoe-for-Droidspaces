# UI framework sources
target_sources(tmoes_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/main_menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/browser_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/office_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/download_tools_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/input_method_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/terminal_app_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/education_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/dev_tools_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/docker_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/backup_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/config_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/termux_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_desktop_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_remote_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/gui_beautify_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/software_center_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/beta_features_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/virtualization_plugin.cpp
)
