function(add_bundled_libwpe_key_unicode)
    if(TARGET wpe_key_unicode)
        return()
    endif()

    set(WPE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libwpe)

    add_library(wpe_key_unicode STATIC EXCLUDE_FROM_ALL ${WPE_SOURCE_DIR}/src/key-unicode.c)
    target_include_directories(wpe_key_unicode PUBLIC ${WPE_SOURCE_DIR}/include)
    target_compile_definitions(wpe_key_unicode
        PUBLIC
            __WPE_H_INSIDE__
            wpe_export_h
            WPE_EXPORT=
        PRIVATE
            WPE_COMPILATION
    )
    set_target_properties(wpe_key_unicode PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
    if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(wpe_key_unicode INTERFACE LINKER:--exclude-libs,libwpe_key_unicode.a)
    endif()
endfunction()

add_bundled_libwpe_key_unicode()
