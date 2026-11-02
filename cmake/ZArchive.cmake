enable_language(C)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/zstd/build/cmake ${CMAKE_BINARY_DIR}/3rdparty/zstd EXCLUDE_FROM_ALL)
set_target_properties(libzstd_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZARCHIVE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/ZArchive)
add_library(zarchive STATIC EXCLUDE_FROM_ALL
    ${ZARCHIVE_SOURCE_DIR}/src/zarchivereader.cpp
    ${ZARCHIVE_SOURCE_DIR}/src/zarchivewriter.cpp
    ${ZARCHIVE_SOURCE_DIR}/src/sha_256.c
)
target_include_directories(zarchive SYSTEM PUBLIC ${ZARCHIVE_SOURCE_DIR}/include)
target_compile_features(zarchive PUBLIC cxx_std_20)
target_link_libraries(zarchive PRIVATE libzstd_static)
set_target_properties(zarchive PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(zarchive INTERFACE LINKER:--exclude-libs,libzarchive.a:libzstd.a)
endif()
