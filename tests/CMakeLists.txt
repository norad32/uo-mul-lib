include(CTest)

find_package(Catch2 CONFIG REQUIRED COMPONENTS Catch2WithMain)
list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
include(Catch)

file(
    GLOB_RECURSE
    test_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp")
if(BUILD_GUI)
    file(
        GLOB_RECURSE
        gui_test_sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gui/*.cpp")
    list(APPEND test_sources ${gui_test_sources})
endif()
add_executable(tests ${test_sources})
target_link_libraries(
    tests
    PRIVATE Catch2::Catch2WithMain
            app
            core
            application_options
            CLI11::CLI11)
apply_common_compile_options(tests)
if(BUILD_GUI)
    target_link_libraries(tests PRIVATE gui)
endif()
apply_unit_test_compile_options(tests)
if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    apply_coverage_compile_options(tests)
    apply_unit_test_link_options(tests)
    foreach(target IN ITEMS core app)
        apply_coverage_compile_options(${target})
    endforeach()
    target_link_options(
        app
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(app INTERFACE gcov)
    if(BUILD_GUI)
        apply_coverage_compile_options(gui)
    endif()
endif()
catch_discover_tests(tests)
add_test(NAME cli_check COMMAND ${EXECUTABLE_NAME} check)
add_test(NAME cli_version COMMAND ${EXECUTABLE_NAME} --version)
if(BUILD_GUI_RUNTIME)
    add_test(NAME gui_cli_version COMMAND ${GUI_EXECUTABLE_NAME} --version)
endif()

add_test(
    NAME package_consumer
    COMMAND
        ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DMAIN_BINARY_DIR=${PROJECT_BINARY_DIR}
        -DBINARY_DIR=${PROJECT_BINARY_DIR}/package-consumer
        -DPREFIX_DIR=${PROJECT_BINARY_DIR}/package-consumer-prefix
        -DCONFIG=$<CONFIG> -P
        ${PROJECT_SOURCE_DIR}/tests/package-consumer/run-package-consumer.cmake)
