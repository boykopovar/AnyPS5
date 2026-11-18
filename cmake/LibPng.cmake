include(${CMAKE_SOURCE_DIR}/cmake/Zlib.cmake)

function(add_bundled_libpng)
    if(TARGET png_backend)
        return()
    endif()

    set(pngSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/libpng)
    set(pngBinaryDir ${CMAKE_BINARY_DIR}/3rdparty/libpng)
    configure_file(${pngSourceDir}/scripts/pnglibconf.h.prebuilt ${pngBinaryDir}/pnglibconf.h COPYONLY)

    add_library(png_backend STATIC EXCLUDE_FROM_ALL
        ${pngSourceDir}/png.c
        ${pngSourceDir}/pngerror.c
        ${pngSourceDir}/pngget.c
        ${pngSourceDir}/pngmem.c
        ${pngSourceDir}/pngpread.c
        ${pngSourceDir}/pngread.c
        ${pngSourceDir}/pngrio.c
        ${pngSourceDir}/pngrtran.c
        ${pngSourceDir}/pngrutil.c
        ${pngSourceDir}/pngset.c
        ${pngSourceDir}/pngtrans.c
        ${pngSourceDir}/pngwio.c
        ${pngSourceDir}/pngwrite.c
        ${pngSourceDir}/pngwtran.c
        ${pngSourceDir}/pngwutil.c)
    target_include_directories(png_backend PUBLIC ${pngSourceDir} ${pngBinaryDir})
    target_compile_options(png_backend PRIVATE -w)
    target_link_libraries(png_backend PUBLIC zlibstatic)
    set_target_properties(png_backend PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden)
    install(FILES ${pngSourceDir}/LICENSE DESTINATION licenses/libpng)
endfunction()

add_bundled_libpng()
