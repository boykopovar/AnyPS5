add_test_executable(agc_nan_speculation_pixel_tests EXCLUDE_FROM_ALL tests/NanSpeculationPixel.cpp)
target_link_libraries(agc_nan_speculation_pixel_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_nan_speculation_pixel_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_nan_speculation_pixel_tests)

if(BUILD_TESTING)
    set_target_properties(agc_nan_speculation_pixel_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_nan_speculation_pixel COMMAND agc_nan_speculation_pixel_tests)
    if(WIN32)
        set_tests_properties(agc_nan_speculation_pixel PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
