add_library(core INTERFACE)
set_target_properties(core PROPERTIES EXPORT_NAME core)
target_link_libraries(core INTERFACE options)
target_include_directories(
    core
    INTERFACE
        $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}>)
