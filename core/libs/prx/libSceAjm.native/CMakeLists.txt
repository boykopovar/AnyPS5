enable_language(C)

set(LIBATRAC9_DIR ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
set(LIBATRAC9_PATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/atrac9)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LIBATRAC9_DIR}/unpack.c)
include(${CMAKE_CURRENT_SOURCE_DIR}/PatchLibAtrac9.cmake)
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
        ${LIBATRAC9_PATCHED_DIR}/unpack.c
        ${LIBATRAC9_DIR}/utility.c
)
target_include_directories(atrac9 PRIVATE ${LIBATRAC9_DIR})
set_target_properties(atrac9 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(libSceAjm.native SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Ajm.cpp
)

target_include_directories(libSceAjm.native PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${LIBATRAC9_DIR}
)
target_link_libraries(libSceAjm.native PRIVATE atrac9 ffmpeg libc libkernel)
configure_windows_unwind(libSceAjm.native)

if(BUILD_TESTING)
    add_test_executable(atrac9_gradient_tests tests/Atrac9Gradient.c ${LIBATRAC9_DIR}/bit_reader.c)
    target_include_directories(atrac9_gradient_tests PRIVATE ${LIBATRAC9_PATCHED_DIR} ${LIBATRAC9_DIR})
    target_compile_options(atrac9_gradient_tests PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(atrac9_gradient_tests PRIVATE LINKER:-dead_strip)
    else()
        target_link_options(atrac9_gradient_tests PRIVATE LINKER:--gc-sections)
    endif()
    add_test(NAME atrac9_gradient COMMAND atrac9_gradient_tests)

    add_test_executable(guest_ajm_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjm.cpp)
    target_include_directories(guest_ajm_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajm_tests PRIVATE libSceAjm.native libc)
    configure_windows_unwind(guest_ajm_tests)
    add_test(NAME guest_ajm COMMAND guest_ajm_tests)
    if(WIN32)
        set_tests_properties(guest_ajm PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAjm.native>")
    endif()
endif()
