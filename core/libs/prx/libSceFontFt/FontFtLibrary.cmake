function(add_sce_font_ft_library target)
    set(fontDir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${fontDir}/Export.cpp
            ${fontDir}/src/Driver.cpp
    )
    target_include_directories(${target} PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE freetype libc)
    set_target_properties(${target} PROPERTIES
            CXX_EXTENSIONS OFF
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
    )
    configure_windows_unwind(${target})
endfunction()
