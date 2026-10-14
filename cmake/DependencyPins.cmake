function(
    application_pin_fetchcontent_dependency
    dependency_name
    repository
    commit)
    string(TOLOWER "${dependency_name}" dependency_prefix)
    set(${dependency_prefix}_VENDORED_SOURCE_DIR
        "${APPLICATION_SOURCE_DIR}/third_party/${dependency_prefix}"
        CACHE PATH "Path to vendored ${dependency_name} sources")
    if(EXISTS "${${dependency_prefix}_VENDORED_SOURCE_DIR}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_${dependency_prefix}
            "${${dependency_prefix}_VENDORED_SOURCE_DIR}"
            CACHE PATH "Use vendored ${dependency_name} sources" FORCE)
    else()
        FetchContent_Declare(
            ${dependency_name}
            GIT_REPOSITORY ${repository}
            GIT_TAG ${commit}
            GIT_SHALLOW FALSE)
        set(FETCHCONTENT_UPDATES_DISCONNECTED_${dependency_prefix}
            ON
            CACHE BOOL "Disable network updates for pinned ${dependency_name}")
    endif()
endfunction()
