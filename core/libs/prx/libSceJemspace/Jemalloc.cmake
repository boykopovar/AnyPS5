enable_language(C)
include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckTypeSize)
find_package(Threads REQUIRED)

get_filename_component(jemspaceSource "${CMAKE_CURRENT_LIST_DIR}/../../../../3rdparty/jemalloc" ABSOLUTE)
set(jemspaceBuild "${CMAKE_CURRENT_BINARY_DIR}/jemalloc")
include(${CMAKE_CURRENT_LIST_DIR}/JemallocConfig.cmake)

set(jemspaceSources
    arena background_thread base bin bitmap ckh ctl div extent extent_dss extent_mmap
    hash hook jemalloc large log malloc_io mutex mutex_pool nstime prng prof rtree
    sc stats sz tcache test_hooks ticker tsd witness)
list(TRANSFORM jemspaceSources PREPEND "${jemspaceSource}/src/")
list(TRANSFORM jemspaceSources APPEND ".c")
add_library(jemspace_symbols OBJECT EXCLUDE_FROM_ALL ${jemspaceSources} GuestPages.c)
add_library(jemspace_allocator STATIC EXCLUDE_FROM_ALL ${jemspaceSources})
foreach(target jemspace_symbols jemspace_allocator)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON C_VISIBILITY_PRESET hidden)
    target_include_directories(${target} PRIVATE "${jemspaceBuild}/include" "${jemspaceSource}/include")
    target_link_libraries(${target} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    if(NOT WIN32)
        target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
    endif()
    if(ANYPS5_ENABLE_ASAN)
        target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    endif()
endforeach()
target_compile_definitions(jemspace_symbols PRIVATE JEMALLOC_NO_PRIVATE_NAMESPACE)
add_custom_command(OUTPUT "${jemspaceBuild}/include/jemalloc/internal/private_namespace.h"
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DOBJECTS=$<JOIN:$<TARGET_OBJECTS:jemspace_symbols>,|>"
        "-DOUTPUT=${jemspaceBuild}/include/jemalloc/internal/private_namespace.h" "-DAPPLE=${APPLE}"
        -P "${CMAKE_CURRENT_LIST_DIR}/JemallocNamespace.cmake"
    DEPENDS jemspace_symbols $<TARGET_OBJECTS:jemspace_symbols> "${CMAKE_CURRENT_LIST_DIR}/JemallocNamespace.cmake"
    VERBATIM)
add_custom_target(jemspace_namespace DEPENDS "${jemspaceBuild}/include/jemalloc/internal/private_namespace.h")
add_dependencies(jemspace_allocator jemspace_namespace)
set_target_properties(jemspace_allocator PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${jemspaceBuild}/include")
target_compile_definitions(jemspace_allocator INTERFACE JEMALLOC_EXPORT=)
