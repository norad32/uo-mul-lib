add_library(
    uomul
    mul_common/mul_common.cpp
    mul_common/output_transaction.cpp
    radarcol/radarcol_common.cpp
    radarcol/radarcol_editor.cpp
    radarcol/radarcol_reader.cpp
    texmaps/texmaps_common.cpp
    texmaps/texmaps_editor.cpp
    texmaps/texmaps_reader.cpp
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
