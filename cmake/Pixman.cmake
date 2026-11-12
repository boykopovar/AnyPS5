function(add_bundled_pixman)
    if(TARGET pixman_backend)
        return()
    endif()

    set(pixmanSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/pixman/pixman)
    set(pixmanBinaryDir ${CMAKE_BINARY_DIR}/3rdparty/pixman)
    file(MAKE_DIRECTORY ${pixmanBinaryDir})

    set(PIXMAN_VERSION_MAJOR 0)
    set(PIXMAN_VERSION_MINOR 42)
    set(PIXMAN_VERSION_MICRO 2)
    configure_file(${pixmanSourceDir}/pixman-version.h.in ${pixmanBinaryDir}/pixman-version.h @ONLY)

    if(WIN32)
        set(pixmanLongSize 4)
    else()
        set(pixmanLongSize 8)
    endif()
    file(CONFIGURE OUTPUT ${pixmanBinaryDir}/config.h CONTENT
        "#define PACKAGE \"pixman\"\n#define USE_SSE2 1\n#define USE_SSSE3 1\n#define TLS __thread\n#define TOOLCHAIN_SUPPORTS_ATTRIBUTE_CONSTRUCTOR 1\n#define HAVE_BUILTIN_CLZ 1\n#define SIZEOF_LONG ${pixmanLongSize}\n")

    add_library(pixman_backend STATIC EXCLUDE_FROM_ALL
        ${pixmanSourceDir}/pixman.c
        ${pixmanSourceDir}/pixman-access.c
        ${pixmanSourceDir}/pixman-access-accessors.c
        ${pixmanSourceDir}/pixman-bits-image.c
        ${pixmanSourceDir}/pixman-combine32.c
        ${pixmanSourceDir}/pixman-combine-float.c
        ${pixmanSourceDir}/pixman-conical-gradient.c
        ${pixmanSourceDir}/pixman-filter.c
        ${pixmanSourceDir}/pixman-x86.c
        ${pixmanSourceDir}/pixman-mips.c
        ${pixmanSourceDir}/pixman-arm.c
        ${pixmanSourceDir}/pixman-ppc.c
        ${pixmanSourceDir}/pixman-edge.c
        ${pixmanSourceDir}/pixman-edge-accessors.c
        ${pixmanSourceDir}/pixman-fast-path.c
        ${pixmanSourceDir}/pixman-glyph.c
        ${pixmanSourceDir}/pixman-general.c
        ${pixmanSourceDir}/pixman-gradient-walker.c
        ${pixmanSourceDir}/pixman-image.c
        ${pixmanSourceDir}/pixman-implementation.c
        ${pixmanSourceDir}/pixman-linear-gradient.c
        ${pixmanSourceDir}/pixman-matrix.c
        ${pixmanSourceDir}/pixman-noop.c
        ${pixmanSourceDir}/pixman-radial-gradient.c
        ${pixmanSourceDir}/pixman-region16.c
        ${pixmanSourceDir}/pixman-region32.c
        ${pixmanSourceDir}/pixman-solid-fill.c
        ${pixmanSourceDir}/pixman-timer.c
        ${pixmanSourceDir}/pixman-trap.c
        ${pixmanSourceDir}/pixman-utils.c
        ${pixmanSourceDir}/pixman-sse2.c
        ${pixmanSourceDir}/pixman-ssse3.c)
    set_source_files_properties(${pixmanSourceDir}/pixman-sse2.c PROPERTIES COMPILE_OPTIONS -msse2)
    set_source_files_properties(${pixmanSourceDir}/pixman-ssse3.c PROPERTIES COMPILE_OPTIONS -mssse3)
    target_include_directories(pixman_backend PUBLIC ${pixmanSourceDir} ${pixmanBinaryDir})
    target_compile_definitions(pixman_backend PRIVATE HAVE_CONFIG_H)
    target_compile_options(pixman_backend PRIVATE -w)
    set_target_properties(pixman_backend PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden)
    install(FILES ${CMAKE_SOURCE_DIR}/3rdparty/pixman/COPYING DESTINATION licenses/pixman)
endfunction()

add_bundled_pixman()
