add_library(image_codec image_codec.cpp)
set_target_properties(image_codec PROPERTIES EXPORT_NAME image_codec)
target_include_directories(
    image_codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PACKAGE_NAME}/image_codec>
)
target_link_libraries(image_codec PUBLIC options uomul)
target_include_directories(
    image_codec SYSTEM
    PRIVATE $<TARGET_PROPERTY:stb_headers,INTERFACE_INCLUDE_DIRECTORIES>)
