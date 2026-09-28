enable_language(C)

set(LIBATRAC9_DIR ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
add_library(atrac9 STATIC EXCLUDE_FROM_ALL
        ${LIBATRAC9_DIR}/band_extension.c
        ${LIBATRAC9_DIR}/bit_allocation.c
        ${LIBATRAC9_DIR}/bit_reader.c
        ${LIBATRAC9_DIR}/decinit.c
        ${LIBATRAC9_DIR}/decoder.c
        ${LIBATRAC9_DIR}/huffCodes.c
        ${LIBATRAC9_DIR}/imdct.c
        ${LIBATRAC9_DIR}/libatrac9.c
        ${LIBATRAC9_DIR}/quantization.c
        ${LIBATRAC9_DIR}/scale_factors.c
        ${LIBATRAC9_DIR}/tables.c
        ${LIBATRAC9_DIR}/unpack.c
        ${LIBATRAC9_DIR}/utility.c
)
set_target_properties(atrac9 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(libSceAjm.native SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Ajm.cpp
)

target_include_directories(libSceAjm.native PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${LIBATRAC9_DIR}
)
target_link_libraries(libSceAjm.native PRIVATE atrac9 libc libkernel)
configure_windows_unwind(libSceAjm.native)
