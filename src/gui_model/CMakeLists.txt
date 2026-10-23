set(GUI_MODEL_SOURCES
    dialogs/dialog_contracts.cpp
    dialogs/dialog_host.cpp
    documents/document_identity.cpp
    documents/document_descriptors.cpp
    documents/map_document.cpp
    documents/radarcol_document.cpp
    persistence/config_paths.cpp
    persistence/asset_state_store.cpp
    persistence/preferences.cpp
    persistence/recent_files.cpp
    profile/data_discovery.cpp
    profile/data_profile.cpp
    profile/data_profile_store.cpp
    profile/data_validation.cpp
    providers/color_conversion.cpp
    providers/data_providers.cpp
    providers/embedded_radarcol.cpp
    layout/panel_registry.cpp
    operations/operation_controller.cpp
    lifecycle/document_lifecycle.cpp
    source/source_identity.cpp
    source/source_locks.cpp
    source/source_validation.cpp
    session/workbench_session.cpp
    input/coordinate_parser.cpp
    model/assets/asset_window_contracts.cpp
    output/output_set_transaction.cpp
    world/map_context.cpp
    world/combine_contracts.cpp
    workspaces/map_workspace.cpp
    workspaces/map_workspace_manager.cpp)

add_library(gui_model ${GUI_MODEL_SOURCES})
set_target_properties(gui_model PROPERTIES EXPORT_NAME gui_model)
target_include_directories(
    gui_model
    PUBLIC
        $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}>)
target_link_libraries(gui_model PUBLIC core uomul options)
