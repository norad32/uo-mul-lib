if(NOT DEFINED SOURCE_DIR
   OR NOT DEFINED MAIN_BINARY_DIR
   OR NOT DEFINED BINARY_DIR
   OR NOT DEFINED PREFIX_DIR)
    message(
        FATAL_ERROR
            "SOURCE_DIR, MAIN_BINARY_DIR, BINARY_DIR, and PREFIX_DIR are required."
    )
endif()

file(REMOVE_RECURSE "${BINARY_DIR}" "${PREFIX_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MAIN_BINARY_DIR}" --prefix
            "${PREFIX_DIR}" --component Development
    RESULT_VARIABLE install_development_result)
if(NOT
   install_development_result
   EQUAL
   0)
    message(FATAL_ERROR "Installing the Development component failed.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MAIN_BINARY_DIR}" --prefix
            "${PREFIX_DIR}" --component Runtime
    RESULT_VARIABLE install_runtime_result)
if(NOT
   install_runtime_result
   EQUAL
   0)
    message(FATAL_ERROR "Installing the Runtime component failed.")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/package-consumer" -B
        "${BINARY_DIR}" -DCMAKE_PREFIX_PATH=${PREFIX_DIR}
        -DCMAKE_BUILD_TYPE=${CONFIG}
    RESULT_VARIABLE configure_result)
if(NOT
   configure_result
   EQUAL
   0)
    message(FATAL_ERROR "Configuring the package consumer failed.")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}"
                RESULT_VARIABLE build_result)
if(NOT
   build_result
   EQUAL
   0)
    message(FATAL_ERROR "Building the package consumer failed.")
endif()
