add_library(core gui_log_sink.cpp log_level.cpp logger.cpp)
set_target_properties(core PROPERTIES EXPORT_NAME core)
target_link_libraries(
    core PUBLIC application_options "$<BUILD_INTERFACE:spdlog::spdlog>"
                "$<INSTALL_INTERFACE:$<TARGET_NAME_IF_EXISTS:spdlog::spdlog>>")
if(ENABLE_COVERAGE)
    target_link_options(
        core
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(core INTERFACE gcov)
endif()
