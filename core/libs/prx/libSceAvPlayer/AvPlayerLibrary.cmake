function(add_sce_avplayer_library target)
    set(avPlayerDir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${avPlayerDir}/Export.cpp
            ${avPlayerDir}/src/Player.cpp
            ${avPlayerDir}/src/Source.cpp
    )
    target_include_directories(${target} PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE ffmpeg libc)
    set_target_properties(${target} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
    )
    configure_windows_unwind(${target})
endfunction()
