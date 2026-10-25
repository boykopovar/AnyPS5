enable_language(C)

set(LIBATRAC9_DIR ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
set(LIBATRAC9_PATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/atrac9)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LIBATRAC9_DIR}/unpack.c ${LIBATRAC9_DIR}/bit_reader.c ${LIBATRAC9_DIR}/decinit.c ${LIBATRAC9_DIR}/utility.c)
include(${CMAKE_CURRENT_SOURCE_DIR}/PatchLibAtrac9.cmake)
add_library(atrac9 STATIC EXCLUDE_FROM_ALL
        ${LIBATRAC9_DIR}/band_extension.c
        ${LIBATRAC9_DIR}/bit_allocation.c
        ${LIBATRAC9_PATCHED_DIR}/bit_reader.c
        ${LIBATRAC9_PATCHED_DIR}/decinit.c
        ${LIBATRAC9_DIR}/decoder.c
        ${LIBATRAC9_DIR}/huffCodes.c
        ${LIBATRAC9_DIR}/imdct.c
        ${LIBATRAC9_DIR}/libatrac9.c
        ${LIBATRAC9_DIR}/quantization.c
        ${LIBATRAC9_DIR}/scale_factors.c
        ${LIBATRAC9_DIR}/tables.c
        ${LIBATRAC9_PATCHED_DIR}/unpack.c
        ${LIBATRAC9_PATCHED_DIR}/utility.c
)
target_include_directories(atrac9 PRIVATE ${LIBATRAC9_DIR})
set_target_properties(atrac9 PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(VGMSTREAM_ATRAC9_DIR ${CMAKE_SOURCE_DIR}/3rdparty/vgmstream/src/coding/libs)
add_library(atrac9_vibration STATIC EXCLUDE_FROM_ALL
        ${VGMSTREAM_ATRAC9_DIR}/atrac9_dec.c
        ${VGMSTREAM_ATRAC9_DIR}/atrac9_internal.c
)
set_target_properties(atrac9_vibration PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

include(${CMAKE_CURRENT_SOURCE_DIR}/AjmLibrary.cmake)
add_sce_ajm_library(libSceAjm.native)

if(BUILD_TESTING)
    add_test_executable(atrac9_gradient_tests tests/Atrac9Gradient.c ${LIBATRAC9_DIR}/bit_reader.c)
    target_include_directories(atrac9_gradient_tests PRIVATE ${LIBATRAC9_PATCHED_DIR} ${LIBATRAC9_DIR})
    target_link_libraries(atrac9_gradient_tests PRIVATE atrac9)
    target_compile_options(atrac9_gradient_tests PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(atrac9_gradient_tests PRIVATE LINKER:-dead_strip)
    else()
        target_link_options(atrac9_gradient_tests PRIVATE LINKER:--gc-sections)
    endif()
    add_test(NAME atrac9_gradient COMMAND atrac9_gradient_tests)

    add_test_executable(atrac9_config_data_tests tests/Atrac9ConfigData.c)
    target_include_directories(atrac9_config_data_tests PRIVATE ${LIBATRAC9_DIR})
    target_link_libraries(atrac9_config_data_tests PRIVATE atrac9)
    if(NOT WIN32)
        target_link_libraries(atrac9_config_data_tests PRIVATE m)
    endif()
    add_test(NAME atrac9_config_data COMMAND atrac9_config_data_tests)

    add_test_executable(guest_ajm_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjm.cpp)
    target_include_directories(guest_ajm_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajm_tests PRIVATE libSceAjm.native libc)
    configure_windows_unwind(guest_ajm_tests)
    add_test(NAME guest_ajm COMMAND guest_ajm_tests)
    add_test_executable(guest_ajm_vibration_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmVibration.cpp)
    target_include_directories(guest_ajm_vibration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajm_vibration_tests PRIVATE libSceAjm.native libc)
    configure_windows_unwind(guest_ajm_vibration_tests)
    add_test(NAME guest_ajm_vibration COMMAND guest_ajm_vibration_tests)
    add_test_executable(guest_ajm_wvorbis_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmWVorbis.cpp)
    target_include_directories(guest_ajm_wvorbis_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajm_wvorbis_tests PRIVATE libSceAjm.native libc)
    configure_windows_unwind(guest_ajm_wvorbis_tests)
    add_test(NAME guest_ajm_wvorbis COMMAND guest_ajm_wvorbis_tests)
    if(UNIX)
        add_test_executable(guest_ajm_teardown_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmTeardown.cpp)
        target_include_directories(guest_ajm_teardown_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_ajm_teardown_tests PRIVATE libc ${CMAKE_DL_LIBS})
        add_dependencies(guest_ajm_teardown_tests libSceAjm.native)
        add_test(NAME guest_ajm_teardown COMMAND guest_ajm_teardown_tests $<TARGET_FILE:libSceAjm.native>)
    endif()
    if(WIN32)
        set_tests_properties(guest_ajm guest_ajm_vibration guest_ajm_wvorbis PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAjm.native>")
    endif()
endif()
