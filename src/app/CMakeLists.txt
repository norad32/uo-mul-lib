add_library(app cli.cpp cli_arguments.cpp)
set_target_properties(app PROPERTIES EXPORT_NAME app)
target_include_directories(
    app
    PUBLIC
        $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}>)
target_link_libraries(app PUBLIC core application_options)
target_include_directories(
    app SYSTEM
    PRIVATE "$<TARGET_PROPERTY:CLI11::CLI11,INTERFACE_INCLUDE_DIRECTORIES>")
if(ENABLE_COVERAGE)
    target_link_options(
        app
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(app INTERFACE gcov)
endif()
