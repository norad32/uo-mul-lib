add_library(
    uomul
    mul_common/mul_common.cpp
    map/map_diff.cpp
    map/map_editor.cpp
    map/map_common.cpp
    map/map_reader.cpp
    map/map_stats.cpp)
set_target_properties(uomul PROPERTIES EXPORT_NAME uomul)
target_link_libraries(uomul PUBLIC application_options)
target_include_directories(
    uomul PUBLIC $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
                 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
