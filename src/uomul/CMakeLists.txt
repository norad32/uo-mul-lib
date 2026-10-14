add_library(uomul map.cpp)
set_target_properties(uomul PROPERTIES EXPORT_NAME uomul)
target_link_libraries(uomul PUBLIC application_options)
target_include_directories(
    uomul PUBLIC $<BUILD_INTERFACE:${APPLICATION_SOURCE_DIR}/src>
                 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
