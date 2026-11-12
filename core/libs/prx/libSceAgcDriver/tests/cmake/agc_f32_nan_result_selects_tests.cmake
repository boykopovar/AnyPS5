add_test_executable(agc_f32_nan_result_selects_tests EXCLUDE_FROM_ALL tests/F32NanResultSelects.cpp)
target_link_libraries(agc_f32_nan_result_selects_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_f32_nan_result_selects_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_f32_nan_result_selects_tests)

if(BUILD_TESTING)
    set_target_properties(agc_f32_nan_result_selects_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_f32_nan_result_selects COMMAND agc_f32_nan_result_selects_tests)
    if(WIN32)
        set_tests_properties(agc_f32_nan_result_selects PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
