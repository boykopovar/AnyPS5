add_test_executable(agc_driver_cmask_fast_clear_tests EXCLUDE_FROM_ALL
        tests/CmaskFastClear.cpp
        ${agcCoverShaderHeaders}
)

target_include_directories(agc_driver_cmask_fast_clear_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_cmask_fast_clear_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_cmask_fast_clear_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_cmask_fast_clear_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_cmask_fast_clear_tests)
endif()
