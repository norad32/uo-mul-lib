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
    application/application.cpp
    application/workbench_state.cpp
    commands/command_registry.cpp
    commands/file_operations.cpp
    commands/menu_bar.cpp
    commands/operation_controller.cpp
    context/coordinate_parser.cpp
    context/color_conversion.cpp
    context/map_context.cpp
    frontend/document_surface.cpp
    assets/asset_window_contracts.cpp
    assets/asset_window.cpp
    strings/ui_strings.cpp
    profile/data_discovery.cpp
    profile/data_profile.cpp
    profile/data_profile_store.cpp
    profile/data_validation.cpp
    profile/workbench_session.cpp
    providers/data_providers.cpp
    providers/embedded_radarcol.cpp
    dialogs/file_size.cpp
    dialogs/file_browser_dialog.cpp
    dialogs/file_selector_dialog.cpp
    dialogs/map_facet_dialog.cpp
    dialogs/preferences_dialog.cpp
    dialogs/dialog_contracts.cpp
    dialogs/dialog_host.cpp
    dialogs/dialog_presentation.cpp
    persistence/config_paths.cpp
    documents/document_identity.cpp
    documents/map_document.cpp
    documents/radarcol_document.cpp
    documents/map_workspace.cpp
    documents/map_workspace_manager.cpp
    persistence/layout_store.cpp
    persistence/preferences.cpp
    persistence/recent_files.cpp
    views/block_inspector_panel.cpp
    views/common.cpp
    views/log_panel.cpp
    views/map_canvas_panel.cpp
    views/panel_registry.cpp
    views/radarcol_panel.cpp
    views/texmaps_panel.cpp
    views/tile_inspector_panel.cpp
    views/texture_preview.cpp
    dialogs/new_file_dialog.cpp
    frontend/frame_composition.cpp
    frontend/welcome_greetings.cpp
    frontend/welcome_screen.cpp)
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
