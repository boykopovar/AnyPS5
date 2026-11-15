if(BUILD_TESTING)
    add_test(NAME agc_driver_f32_arithmetic_nan_fast COMMAND agc_driver_f32_arithmetic_nan_tests)
    set_tests_properties(agc_driver_f32_arithmetic_nan_fast PROPERTIES ENVIRONMENT "APS5_FAST_F32=1;ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_f32_arithmetic_nan_fast PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
