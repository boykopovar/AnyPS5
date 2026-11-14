function(add_bundled_zlib)
    if(TARGET zlibstatic)
        return()
    endif()

    set(ZLIB_BUILD_TESTING OFF)
    set(ZLIB_BUILD_SHARED OFF)
    set(ZLIB_BUILD_STATIC ON)
    set(ZLIB_INSTALL OFF)
    add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/zlib ${CMAKE_BINARY_DIR}/3rdparty/zlib EXCLUDE_FROM_ALL)
    set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
endfunction()

add_bundled_zlib()
