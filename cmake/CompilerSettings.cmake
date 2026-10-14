include_guard(GLOBAL)

set(WARNING_OPTIONS
    -Werror
    -Wpedantic
    -Wall
    -Wextra
    -Wmissing-declarations
    -Wconversion
    -Wshadow
    -Wlogical-op
    -Wfloat-equal)

set(DEBUGGING_OPTIONS -g3)
set(COMMON_OPT_OPTIONS -ffunction-sections -fdata-sections)
set(DEBUG_OPT_OPTIONS ${COMMON_OPT_OPTIONS} -Og)
set(COVERAGE_COMPILE_OPTIONS -fprofile-arcs -ftest-coverage)
set(UNIT_TEST_COMPILE_OPTIONS -fno-elide-constructors)
set(RELEASE_OPT_OPTIONS ${COMMON_OPT_OPTIONS} -O2)
set(DIAGMSG_OPTIONS -fmessage-length=0)

function(apply_common_compile_options target_name)
    if(NOT TARGET ${target_name})
        message(
            FATAL_ERROR
                "apply_common_compile_options: target does not exist: ${target_name}"
        )
    endif()

    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(scope INTERFACE)
    else()
        set(scope PRIVATE)
    endif()

    target_compile_options(
        ${target_name}
        ${scope}
        ${WARNING_OPTIONS}
        ${DIAGMSG_OPTIONS}
        $<$<CONFIG:Debug>:${DEBUG_OPT_OPTIONS}>
        $<$<CONFIG:Debug>:${DEBUGGING_OPTIONS}>
        $<$<CONFIG:Release>:${RELEASE_OPT_OPTIONS}>)
    target_compile_definitions(
        ${target_name}
        ${scope}
        $<$<CONFIG:Debug>:IM_CPP_DEBUG_BUILD=1>
        $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
        $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
        $<$<CONFIG:Release>:NDEBUG=1>
        $<$<CONFIG:Release>:RELEASE=1>)
endfunction()

function(apply_coverage_compile_options target_name)
    if(NOT TARGET ${target_name})
        message(
            FATAL_ERROR
                "apply_coverage_compile_options: target does not exist: ${target_name}"
        )
    endif()

    target_compile_options(${target_name} PRIVATE ${COVERAGE_COMPILE_OPTIONS})
endfunction()

function(apply_unit_test_compile_options target_name)
    if(NOT TARGET ${target_name})
        message(
            FATAL_ERROR
                "apply_unit_test_compile_options: target does not exist: ${target_name}"
        )
    endif()

    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(scope INTERFACE)
    else()
        set(scope PRIVATE)
    endif()

    target_compile_options(${target_name} ${scope} ${UNIT_TEST_COMPILE_OPTIONS})
endfunction()

function(apply_unit_test_link_options target_name)
    if(NOT TARGET ${target_name})
        message(
            FATAL_ERROR
                "apply_unit_test_link_options: target does not exist: ${target_name}"
        )
    endif()

    get_target_property(target_type ${target_name} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        target_link_options(
            ${target_name}
            INTERFACE
            -fprofile-arcs
            -ftest-coverage)
        target_link_libraries(${target_name} INTERFACE gcov)
    else()
        target_link_options(
            ${target_name}
            PRIVATE
            -fprofile-arcs
            -ftest-coverage)
        target_link_libraries(${target_name} PRIVATE gcov)
    endif()
endfunction()
