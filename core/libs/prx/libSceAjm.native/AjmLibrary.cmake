function(add_sce_ajm_library target)
    set(ajmDir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_library(${target} SHARED EXCLUDE_FROM_ALL
            ${ajmDir}/Export.cpp
            ${ajmDir}/src/Ajm.cpp
            ${ARGN}
    )
    target_include_directories(${target} PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src
            ${CMAKE_SOURCE_DIR}/3rdparty/vgmstream/src/coding/libs
    )
    target_link_libraries(${target} PRIVATE atrac9 atrac9_vibration ffmpeg libc libkernel)
    configure_windows_unwind(${target})
endfunction()
