add_library(
    uomul
    src/common/facet_sizes.cpp
    src/map/map_common.cpp
    src/map/map_diff.cpp
    src/map/map_editor.cpp
    src/map/map_reader.cpp
    src/map/map_stats.cpp
    src/mul_common/mul_common.cpp
    src/mul_common/output_transaction.cpp
    src/radarcol/radarcol_common.cpp
    src/radarcol/radarcol_editor.cpp
    src/radarcol/radarcol_reader.cpp
    src/texmaps/texmaps_common.cpp
    src/texmaps/texmaps_editor.cpp
    src/texmaps/texmaps_reader.cpp
    src/tiledata/tiledata_common.cpp
    src/tiledata/tiledata_editor.cpp
    src/tiledata/tiledata_reader.cpp)
set_target_properties(uomul PROPERTIES EXPORT_NAME uomul)
target_link_libraries(uomul PUBLIC options)
target_include_directories(
    uomul
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${APPLICATION_SOURCE_DIR}/src)
if(ENABLE_COVERAGE)
    target_link_options(
        uomul
        INTERFACE
        -fprofile-arcs
        -ftest-coverage)
    target_link_libraries(uomul INTERFACE gcov)
endif()
