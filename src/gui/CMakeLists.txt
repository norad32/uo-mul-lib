find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(
        GLFW3
        REQUIRED
        IMPORTED_TARGET
        glfw3)
    add_library(glfw ALIAS PkgConfig::GLFW3)
endif()
include(GuiDependencies)

add_library(
    gui
    application.cpp
    data_discovery.cpp
    data_profile.cpp
    data_profile_store.cpp
    data_validation.cpp
    dialogs/data_paths_dialog.cpp
    log_panel.cpp
    map_workspace.cpp
    menu_bar.cpp
    new_map_dialog.cpp
    panels/block_inspector_panel.cpp
    panels/common.cpp
    panels/edit_tools_panel.cpp
    panels/map_canvas_panel.cpp
    panels/map_info_panel.cpp
    panels/radarcol_panel.cpp
    panels/selection_stats_panel.cpp
    panels/texmaps_panel.cpp
    panels/tile_inspector_panel.cpp
    preferences.cpp)
set_target_properties(gui PROPERTIES EXPORT_NAME gui)
set_target_properties(imgui PROPERTIES EXPORT_NAME imgui)
target_include_directories(
    gui
    PUBLIC
        $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}>
        $<BUILD_INTERFACE:${IMGUI_SOURCE_DIR}>
        $<BUILD_INTERFACE:${IMGUI_SOURCE_DIR}/backends>)
target_link_libraries(
    gui
    PUBLIC imgui
           glfw
           OpenGL::GL
           core
           uomul
           application_options)
file(
    RELATIVE_PATH
    APPLICATION_ICON_INSTALL_RELATIVE_PATH
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${DISTRIBUTION_NAME}/app_icon.xpm"
)
target_compile_definitions(
    gui
    PRIVATE
        APPLICATION_ICON_INSTALL_RELATIVE_PATH="${APPLICATION_ICON_INSTALL_RELATIVE_PATH}"
        APPLICATION_ICON_SOURCE_PATH="${APPLICATION_SOURCE_DIR}/assets/app_icon.xpm"
)
if(ENABLE_COVERAGE)
    target_link_options(
        gui
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(gui INTERFACE gcov)
endif()

if(BUILD_GUI_RUNTIME)
    add_executable(${GUI_EXECUTABLE_NAME}
                   ${APPLICATION_SOURCE_DIR}/src/gui_main.cpp)
    target_link_libraries(
        ${GUI_EXECUTABLE_NAME}
        PRIVATE app
                application_options
                gui
                CLI11::CLI11)
    target_compile_definitions(${GUI_EXECUTABLE_NAME} PRIVATE HAS_GUI=1)
endif()
