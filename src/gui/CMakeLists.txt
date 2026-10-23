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
    commands/command_registry.cpp
    commands/menu_bar.cpp
    ui/dialogs/dialog_contracts.cpp
    ui/dialogs/dialog_host.cpp
    ui/dialogs/dialog_presentation.cpp
    ui/dialogs/file_browser_dialog.cpp
    ui/dialogs/file_selector_dialog.cpp
    ui/dialogs/map_facet_dialog.cpp
    ui/dialogs/map_workflow_dialog.cpp
    ui/dialogs/preferences_dialog.cpp
    documents/document_identity.cpp
    documents/map_document.cpp
    documents/radarcol_document.cpp
    persistence/config_paths.cpp
    persistence/layout_store.cpp
    persistence/preferences.cpp
    persistence/recent_files.cpp
    profile/data_discovery.cpp
    profile/data_profile.cpp
    profile/data_profile_store.cpp
    profile/data_validation.cpp
    providers/color_conversion.cpp
    providers/data_providers.cpp
    providers/embedded_radarcol.cpp
    ui/strings/ui_strings.cpp
    layout/panel_registry.cpp
    operations/operation_controller.cpp
    session/workbench_session.cpp
    state/workbench_state.cpp
    ui/documents/document_window.cpp
    ui/frame/workbench_frame.cpp
    ui/input/coordinate_parser.cpp
    ui/formatting/file_size.cpp
    ui/panels/block_inspector_panel.cpp
    ui/panels/tile_inspector_panel.cpp
    ui/panels/log_panel.cpp
    ui/panels/radarcol_panel.cpp
    ui/panels/texmap_panel.cpp
    ui/shell/welcome_greetings.cpp
    ui/shell/welcome_screen.cpp
    ui/widgets/asset_browser.cpp
    ui/widgets/asset_browser_contracts.cpp
    ui/widgets/texture_preview.cpp
    ui/world/map_canvas_view.cpp
    ui/world/tile_labels.cpp
    workflows/file_operations.cpp
    world/map_context.cpp
    workspaces/map_workspace.cpp
    workspaces/map_workspace_manager.cpp)
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
