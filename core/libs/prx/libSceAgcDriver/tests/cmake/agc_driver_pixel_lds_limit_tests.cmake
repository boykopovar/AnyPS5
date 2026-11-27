if(BUILD_TESTING)
    add_test(NAME agc_driver_pixel_lds_limit_parallel_atlas COMMAND agc_driver_pixel_lds_limit_tests --depth-only)
    set_tests_properties(agc_driver_pixel_lds_limit_parallel_atlas PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PARALLEL_ATLAS=1" TIMEOUT 60 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pixel_lds_limit_parallel_atlas PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
