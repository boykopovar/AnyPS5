function(add_sce_audiodec_library target)
    set(audiodecDir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${audiodecDir}/Export.cpp
            ${audiodecDir}/src/Codecs.cpp
    )
    target_include_directories(${target} PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src
    )
    target_link_libraries(${target} PRIVATE atrac9 ffmpeg libc libkernel)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(${target})
endfunction()
