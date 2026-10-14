cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectMetadata.cmake")
set(APPLICATION_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

project(
    "${CMAKE_PACKAGE_NAME}"
    VERSION "${PROJECT_VERSION}"
    DESCRIPTION "${PROJECT_DESCRIPTION}"
    HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}"
    LANGUAGES CXX)

if(NOT
   CMAKE_CXX_COMPILER_ID
   STREQUAL
   "GNU")
    message(
        FATAL_ERROR
            "${DISTRIBUTION_NAME} requires GCC ${MINIMUM_GCC_VERSION} or newer. Select a matching CMAKE_CXX_COMPILER."
    )
endif()
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS MINIMUM_GCC_VERSION)
    message(
        FATAL_ERROR
            "${DISTRIBUTION_NAME} requires GCC ${MINIMUM_GCC_VERSION} or newer; found ${CMAKE_CXX_COMPILER_VERSION}."
    )
endif()

option(ENABLE_CLANG_TIDY
       "Run clang-tidy during compilation and fail on diagnostics" OFF)
option(BUILD_TESTS "Build and register unit tests" OFF)
option(ENABLE_COVERAGE "Instrument project code for Debug gcovr reports" OFF)
option(BUILD_GUI "Build the ImGui GLFW/OpenGL desktop GUI" ON)
option(BUILD_GUI_RUNTIME "Build the separately packaged GUI runtime executable"
       OFF)
option(INSTALL_GUI
       "Install GUI desktop integration with the runtime executable" OFF)
if(BUILD_GUI_RUNTIME AND NOT BUILD_GUI)
    message(FATAL_ERROR "BUILD_GUI_RUNTIME requires BUILD_GUI=ON.")
endif()
if(INSTALL_GUI AND NOT BUILD_GUI)
    message(FATAL_ERROR "INSTALL_GUI requires BUILD_GUI=ON.")
endif()
set(PACKAGE_NAME
    "${DISTRIBUTION_NAME}"
    CACHE STRING "CPack package name")
set_property(CACHE PACKAGE_NAME PROPERTY STRINGS "${DISTRIBUTION_NAME}"
                                         "${DISTRIBUTION_NAME}-gui")
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CompilerSettings)
include(DependencyPins)
set(INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PACKAGE_NAME}")
set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_METADATA_HEADER "${GENERATED_INCLUDE_DIR}/core/app_metadata.h")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}/core")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/app_metadata.h.in"
               "${GENERATED_METADATA_HEADER}" @ONLY)
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/packaging")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GeneratePackagingMetadata.cmake")

add_library(application_options INTERFACE)

set_target_properties(application_options PROPERTIES EXPORT_NAME options)
target_compile_features(application_options
                        INTERFACE cxx_std_${PROJECT_CXX_STANDARD})
set_target_properties(application_options PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    application_options
    INTERFACE
        $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}>)

find_package(
    spdlog
    ${SPDLOG_MINIMUM_VERSION}
    CONFIG
    QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    set(SPDLOG_BUILD_TESTS
        OFF
        CACHE BOOL "Disable spdlog tests" FORCE)
    set(SPDLOG_BUILD_EXAMPLE
        OFF
        CACHE BOOL "Disable spdlog examples" FORCE)
    set(SPDLOG_BUILD_TESTS
        OFF
        CACHE BOOL "Disable spdlog tests" FORCE)
    set(SPDLOG_BUILD_EXAMPLE
        OFF
        CACHE BOOL "Disable spdlog examples" FORCE)
    application_pin_fetchcontent_dependency(
        spdlog https://github.com/gabime/spdlog.git
        79524ddd08a4ec981b7fea76afd08ee05f83755d) # spdlog v1.17.0
    FetchContent_MakeAvailable(spdlog)
endif()

find_package(
    CLI11
    ${CLI11_PACKAGE_VERSION}
    EXACT
    CONFIG
    QUIET)
if(NOT CLI11_FOUND)
    include(FetchContent)
    set(CLI11_BUILD_TESTS
        OFF
        CACHE BOOL "Disable CLI11 tests" FORCE)
    application_pin_fetchcontent_dependency(
        CLI11 https://github.com/CLIUtils/CLI11.git
        6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9) # CLI11 v2.4.2
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    FetchContent_MakeAvailable(CLI11)
    unset(CMAKE_POLICY_VERSION_MINIMUM)
endif()

add_subdirectory(src/core)
add_subdirectory(src/app)

add_executable(${EXECUTABLE_NAME} "${APPLICATION_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${EXECUTABLE_NAME} PRIVATE app)
if(BUILD_GUI)
    add_subdirectory(src/gui)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE gui)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAS_GUI=1)
else()
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE HAS_GUI=0)
endif()

set(compile_targets core app ${EXECUTABLE_NAME})

if(BUILD_GUI)
    list(APPEND compile_targets gui)
    if(BUILD_GUI_RUNTIME)
        list(APPEND compile_targets ${GUI_EXECUTABLE_NAME})
    endif()
endif()
foreach(target IN LISTS compile_targets)
    apply_common_compile_options(${target})
endforeach()

if(BUILD_TESTS)
    include(CTest)
    find_package(Catch2 CONFIG REQUIRED COMPONENTS Catch2WithMain)
    add_subdirectory(tests "${PROJECT_BINARY_DIR}/unit-tests")
endif()

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(clang_tidy_targets core app ${EXECUTABLE_NAME})
    if(BUILD_GUI)
        list(APPEND clang_tidy_targets gui)
        if(BUILD_GUI_RUNTIME)
            list(APPEND clang_tidy_targets ${GUI_EXECUTABLE_NAME})
        endif()
    endif()
    foreach(target IN LISTS clang_tidy_targets)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY
                                                   "${CLANG_TIDY_EXECUTABLE}")
    endforeach()
endif()

include(InstallRules)
