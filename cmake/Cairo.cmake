include(${CMAKE_SOURCE_DIR}/cmake/Pixman.cmake)

function(add_bundled_cairo)
    if(TARGET cairo_backend)
        return()
    endif()

    find_package(Threads REQUIRED)

    set(cairoSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/cairo/src)
    set(cairoBinaryDir ${CMAKE_BINARY_DIR}/3rdparty/cairo)
    file(MAKE_DIRECTORY ${cairoBinaryDir})

    file(CONFIGURE OUTPUT ${cairoBinaryDir}/cairo-features.h CONTENT
        "#ifndef CAIRO_FEATURES_H\n#define CAIRO_FEATURES_H\n#define CAIRO_HAS_IMAGE_SURFACE 1\n#define CAIRO_HAS_USER_FONT 1\n#define CAIRO_HAS_MIME_SURFACE 1\n#define CAIRO_HAS_RECORDING_SURFACE 1\n#define CAIRO_HAS_OBSERVER_SURFACE 1\n#endif\n")

    if(WIN32)
        set(cairoLongSize 4)
        set(cairoThreads "")
    else()
        set(cairoLongSize 8)
        set(cairoThreads "#define CAIRO_HAS_PTHREAD 1\n#define CAIRO_HAS_REAL_PTHREAD 1\n#define HAVE_STRNDUP 1\n")
    endif()
    file(CONFIGURE OUTPUT ${cairoBinaryDir}/config.h CONTENT
        "#define SIZEOF_VOID_P 8\n#define SIZEOF_INT 4\n#define SIZEOF_LONG ${cairoLongSize}\n#define SIZEOF_LONG_LONG 8\n#define SIZEOF_SIZE_T 8\n#define HAVE_STDINT_H 1\n#define HAVE_INTTYPES_H 1\n#define HAVE_UINT64_T 1\n#define HAVE___UINT128_T 1\n#define HAVE_CXX11_ATOMIC_PRIMITIVES 1\n#define HAS_PIXMAN_GLYPHS 1\n${cairoThreads}")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${cairoSourceDir}/cairo.h)
    file(STRINGS ${cairoSourceDir}/cairo.h cairoDeclarations REGEX "^cairo_[a-z0-9_]+ +\\(")
    set(cairoRenames "")
    set(cairoRestores "")
    foreach(declaration IN LISTS cairoDeclarations)
        string(REGEX MATCH "^cairo_[a-z0-9_]+" function "${declaration}")
        string(APPEND cairoRenames "#define ${function} aps5_${function}\n")
        string(APPEND cairoRestores "#undef ${function}\n")
    endforeach()
    file(CONFIGURE OUTPUT ${cairoBinaryDir}/CairoRename.h CONTENT "#pragma once\n${cairoRenames}")
    file(CONFIGURE OUTPUT ${cairoBinaryDir}/CairoBackend.h CONTENT
        "#pragma once\n#define CAIRO_DEPRECATED_H\n#include \"CairoRename.h\"\n#include <cairo.h>\n${cairoRestores}")

    set(cairoSources
        cairo-analysis-surface.c cairo-arc.c cairo-array.c cairo-atomic.c cairo-base64-stream.c
        cairo-base85-stream.c cairo-bentley-ottmann-rectangular.c cairo-bentley-ottmann-rectilinear.c
        cairo-bentley-ottmann.c cairo-botor-scan-converter.c cairo-boxes-intersect.c cairo-boxes.c
        cairo-cache.c cairo-clip-boxes.c cairo-clip-polygon.c cairo-clip-region.c cairo-clip-surface.c
        cairo-clip-tor-scan-converter.c cairo-clip.c cairo-color.c cairo-composite-rectangles.c
        cairo-compositor.c cairo-contour.c cairo-damage.c cairo-debug.c cairo-default-context.c
        cairo-device.c cairo-error.c cairo-fallback-compositor.c cairo-fixed.c
        cairo-font-face-twin-data.c cairo-font-face-twin.c cairo-font-face.c cairo-font-options.c
        cairo-freed-pool.c cairo-freelist.c cairo-gstate.c cairo-hash.c cairo-hull.c
        cairo-image-compositor.c cairo-image-info.c cairo-image-source.c cairo-image-surface.c
        cairo-line.c cairo-lzw.c cairo-mask-compositor.c cairo-matrix.c cairo-mempool.c
        cairo-mesh-pattern-rasterizer.c cairo-misc.c cairo-mono-scan-converter.c cairo-mutex.c
        cairo-no-compositor.c cairo-observer.c cairo-output-stream.c cairo-paginated-surface.c
        cairo-path-bounds.c cairo-path-fill.c cairo-path-fixed.c cairo-path-in-fill.c
        cairo-path-stroke-boxes.c cairo-path-stroke-polygon.c cairo-path-stroke-traps.c
        cairo-path-stroke-tristrip.c cairo-path-stroke.c cairo-path.c cairo-pattern.c cairo-pen.c
        cairo-polygon-intersect.c cairo-polygon-reduce.c cairo-polygon.c cairo-raster-source-pattern.c
        cairo-recording-surface.c cairo-rectangle.c cairo-rectangular-scan-converter.c cairo-region.c
        cairo-rtree.c cairo-scaled-font.c cairo-shape-mask-compositor.c cairo-slope.c
        cairo-spans-compositor.c cairo-spans.c cairo-spline.c cairo-stroke-dash.c cairo-stroke-style.c
        cairo-surface-clipper.c cairo-surface-fallback.c cairo-surface-observer.c cairo-surface-offset.c
        cairo-surface-snapshot.c cairo-surface-subsurface.c cairo-surface-wrapper.c cairo-surface.c
        cairo-time.c cairo-tor-scan-converter.c cairo-tor22-scan-converter.c cairo-toy-font-face.c
        cairo-traps-compositor.c cairo-traps.c cairo-tristrip.c cairo-unicode.c cairo-user-font.c
        cairo-version.c cairo-wideint.c cairo.c cairo-tag-attributes.c cairo-tag-stack.c)
    list(TRANSFORM cairoSources PREPEND ${cairoSourceDir}/)

    add_library(cairo_backend STATIC EXCLUDE_FROM_ALL ${cairoSources})
    target_include_directories(cairo_backend PUBLIC ${cairoSourceDir} ${cairoBinaryDir})
    target_compile_definitions(cairo_backend PRIVATE HAVE_CONFIG_H CAIRO_COMPILATION)
    if(WIN32)
        target_compile_definitions(cairo_backend PUBLIC CAIRO_WIN32_STATIC_BUILD)
    endif()
    target_compile_options(cairo_backend PRIVATE -w -fno-strict-aliasing -include ${cairoBinaryDir}/CairoRename.h)
    target_link_libraries(cairo_backend PUBLIC pixman_backend Threads::Threads)
    if(NOT WIN32 AND NOT APPLE)
        target_link_libraries(cairo_backend PUBLIC m)
    endif()
    set_target_properties(cairo_backend PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden)
    install(FILES
        ${CMAKE_SOURCE_DIR}/3rdparty/cairo/COPYING
        ${CMAKE_SOURCE_DIR}/3rdparty/cairo/COPYING-LGPL-2.1
        ${CMAKE_SOURCE_DIR}/3rdparty/cairo/COPYING-MPL-1.1
        DESTINATION licenses/cairo)
endfunction()

add_bundled_cairo()
