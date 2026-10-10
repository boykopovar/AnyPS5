function(add_sce_font_library target)
    set(fontDir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${fontDir}/src/Layout.cpp
            ${fontDir}/src/Library.cpp
            ${fontDir}/src/Open.cpp
            ${fontDir}/src/Render.cpp
            ${fontDir}/src/State.cpp
            ${fontDir}/src/Style.cpp
            ${fontDir}/src/SystemFont.cpp
            ${fontDir}/src/Text.cpp
            ${fontDir}/src/Unimplemented.cpp
            ${fontDir}/src/WritingLine.cpp
            ${fontDir}/src/Font.cpp
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
