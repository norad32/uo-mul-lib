include("${CMAKE_CURRENT_LIST_DIR}/ProjectMetadata.cmake")

cmake_minimum_required(VERSION ${MINIMUM_CMAKE_VERSION})

if(NOT OUTPUT_DIR)
    message(FATAL_ERROR "Set OUTPUT_DIR to the generated packaging directory.")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}/debian" "${OUTPUT_DIR}/arch")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../packaging/debian/control"
               "${OUTPUT_DIR}/debian/control" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../packaging/arch/PKGBUILD"
               "${OUTPUT_DIR}/arch/PKGBUILD" @ONLY)

foreach(manifest IN ITEMS "${OUTPUT_DIR}/debian/control"
                          "${OUTPUT_DIR}/arch/PKGBUILD")
    file(READ "${manifest}" manifest_contents)
    if(manifest_contents MATCHES "@[A-Za-z_][A-Za-z0-9_]*@")
        message(FATAL_ERROR "Unresolved packaging metadata in ${manifest}.")
    endif()
endforeach()
