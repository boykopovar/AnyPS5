function(add_sce_ngs2_library target)
    set(ngs2Dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${ngs2Dir}/src/Atrac9.cpp
            ${ngs2Dir}/src/Custom.cpp
            ${ngs2Dir}/src/Pan.cpp
            ${ngs2Dir}/src/Rack.cpp
            ${ngs2Dir}/src/Render.cpp
            ${ngs2Dir}/src/Reverb.cpp
            ${ngs2Dir}/src/System.cpp
            ${ngs2Dir}/src/Unimplemented.cpp
            ${ngs2Dir}/src/Voice.cpp
    )
    target_include_directories(${target} PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
    target_link_libraries(${target} PRIVATE atrac9 libc libkernel)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(${target} PRIVATE LINKER:--exclude-libs,ALL)
    endif()
    set_target_properties(${target} PROPERTIES
            CXX_EXTENSIONS OFF
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
    )
    configure_windows_unwind(${target})
endfunction()
