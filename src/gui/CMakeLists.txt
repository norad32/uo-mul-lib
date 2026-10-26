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
    application/gui_resources.cpp
    commands/command_registry.cpp
    commands/menu_bar.cpp
    presentation/dialog_presentation.cpp
    presentation/imgui_fonts.cpp
    presentation/iconography.cpp
    dialogs/file_browser_dialog.cpp
    dialogs/file_selector_dialog.cpp
    dialogs/map_facet_dialog.cpp
    dialogs/map_workflow_dialog.cpp
    dialogs/preferences_dialog.cpp
    application/layout_store.cpp
    presentation/strings.cpp
    workbench/workbench_state.cpp
    workbench/tool_context.cpp
    documents/document_window_name.cpp
    workbench/workbench.cpp
    presentation/file_size.cpp
    panels/block_inspector_panel.cpp
    presentation/asset_browser_helpers.cpp
    panels/icon_gallery_panel.cpp
    panels/tile_inspector_panel.cpp
    panels/log_panel.cpp
    documents/radarcol_document_window.cpp
    documents/texmaps_document_window.cpp
    documents/tiledata_document_window.cpp
    panels/tool_palette_panel.cpp
    application/welcome_greetings.cpp
    application/welcome_view.cpp
    presentation/asset_browser.cpp
    presentation/texture_preview.cpp
    presentation/map_texture_cache.cpp
    documents/map_document_window.cpp
    documents/map_tile_labels.cpp
    workflows/file_workflow.cpp
    workflows/texmap_export_workflow.cpp
    workflows/texmap_replacement_workflow.cpp)
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
           gui_model
           image_codec
           logger
           options)
file(
    RELATIVE_PATH
    APPLICATION_ICON_INSTALL_RELATIVE_PATH
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${DISTRIBUTION_NAME}/app_icon.xpm"
)
file(
    RELATIVE_PATH
    MATERIAL_DESIGN_ICONS_INSTALL_RELATIVE_PATH
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${DISTRIBUTION_NAME}/fonts/materialdesignicons-webfont.ttf"
)
target_compile_definitions(
    gui
    PRIVATE
        APPLICATION_ICON_INSTALL_RELATIVE_PATH="${APPLICATION_ICON_INSTALL_RELATIVE_PATH}"
        APPLICATION_ICON_SOURCE_PATH="${APPLICATION_SOURCE_DIR}/assets/app_icon.xpm"
        MATERIAL_DESIGN_ICONS_INSTALL_RELATIVE_PATH="${MATERIAL_DESIGN_ICONS_INSTALL_RELATIVE_PATH}"
        MATERIAL_DESIGN_ICONS_SOURCE_PATH="${APPLICATION_SOURCE_DIR}/assets/fonts/materialdesignicons-webfont.ttf"
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
                options
                gui
                CLI11::CLI11)
    target_compile_definitions(${GUI_EXECUTABLE_NAME} PRIVATE HAS_GUI=1)
endif()
