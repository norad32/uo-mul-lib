include(CTest)

find_package(Catch2 CONFIG REQUIRED COMPONENTS Catch2WithMain)
list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
include(Catch)

if(BUILD_CORE_TESTS)
        file(
            GLOB_RECURSE
            core_test_sources
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/uomul/*.cpp")
    add_executable(uomul_tests ${core_test_sources})
    target_link_libraries(
        uomul_tests
        PRIVATE Catch2::Catch2WithMain
                uomul
                core
                logger
                options)
    target_compile_definitions(uomul_tests PRIVATE UOMUL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    apply_common_compile_options(uomul_tests)
    apply_unit_test_compile_options(uomul_tests)
    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        apply_coverage_compile_options(uomul_tests)
        apply_unit_test_link_options(uomul_tests)
        foreach(target IN ITEMS core logger uomul)
            apply_coverage_compile_options(${target})
        endforeach()
    endif()
    catch_discover_tests(uomul_tests)
endif()

if(BUILD_CLI_TESTS AND (BUILD_CLI OR BUILD_GUI))
    file(
        GLOB_RECURSE
        cli_test_sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp")
    add_executable(cli_tests ${cli_test_sources})
    target_link_libraries(
        cli_tests
        PRIVATE Catch2::Catch2WithMain
                app
                core
                logger
                options
                CLI11::CLI11)
    apply_common_compile_options(cli_tests)
    apply_unit_test_compile_options(cli_tests)
    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        apply_coverage_compile_options(cli_tests)
        apply_unit_test_link_options(cli_tests)
        apply_coverage_compile_options(app)
        target_link_options(
            app
            INTERFACE
            -fprofile-arcs
            -ftest-coverage)
        target_link_libraries(app INTERFACE gcov)
    endif()
    catch_discover_tests(cli_tests)
endif()

if(BUILD_GUI_TESTS AND BUILD_GUI)
    file(
        GLOB_RECURSE
        gui_test_sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gui/*.cpp")
    add_executable(gui_tests ${gui_test_sources})
    target_link_libraries(
        gui_tests
        PRIVATE Catch2::Catch2WithMain
                gui
                core
                logger
                uomul
                options)
    target_compile_definitions(gui_tests PRIVATE UOMUL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    apply_common_compile_options(gui_tests)
    apply_unit_test_compile_options(gui_tests)
    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        apply_coverage_compile_options(gui_tests)
        apply_unit_test_link_options(gui_tests)
        apply_coverage_compile_options(gui)
    endif()
    catch_discover_tests(gui_tests)
endif()

if(BUILD_GUI_TESTS)
    file(
        GLOB_RECURSE
        gui_model_test_sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gui_model/*.cpp")
    add_executable(gui_model_tests ${gui_model_test_sources})
    target_link_libraries(
        gui_model_tests
        PRIVATE Catch2::Catch2WithMain
                gui_model
                core
                logger
                uomul
                options)
    target_compile_definitions(gui_model_tests PRIVATE UOMUL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    apply_common_compile_options(gui_model_tests)
    apply_unit_test_compile_options(gui_model_tests)
    catch_discover_tests(gui_model_tests)
endif()

if(BUILD_CLI OR BUILD_GUI)
    add_test(NAME cli_check COMMAND ${EXECUTABLE_NAME} check)
    add_test(NAME cli_version COMMAND ${EXECUTABLE_NAME} --version)
endif()
if(BUILD_GUI_RUNTIME)
    add_test(NAME gui_cli_version COMMAND ${GUI_EXECUTABLE_NAME} --version)
endif()

if(BUILD_CLI OR BUILD_GUI)
    add_test(
        NAME package_consumer
        COMMAND
            ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DMAIN_BINARY_DIR=${PROJECT_BINARY_DIR}
            -DBINARY_DIR=${PROJECT_BINARY_DIR}/package-consumer
            -DPREFIX_DIR=${PROJECT_BINARY_DIR}/package-consumer-prefix
            -DCONFIG=$<CONFIG> -P
            ${PROJECT_SOURCE_DIR}/tests/package-consumer/run-package-consumer.cmake
    )
endif()
