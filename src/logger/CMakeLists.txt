add_library(logger gui_log_sink.cpp log_level.cpp logger.cpp)
set_target_properties(logger PROPERTIES EXPORT_NAME logger)
target_link_libraries(
    logger
    PUBLIC core
           options
           "$<BUILD_INTERFACE:spdlog::spdlog>"
           "$<INSTALL_INTERFACE:$<TARGET_NAME_IF_EXISTS:spdlog::spdlog>>")
if(ENABLE_COVERAGE)
    target_link_options(
        logger
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(logger INTERFACE gcov)
endif()
