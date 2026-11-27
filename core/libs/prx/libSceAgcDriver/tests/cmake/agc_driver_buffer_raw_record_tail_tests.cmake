if(BUILD_TESTING)
    add_test(NAME agc_driver_buffer_raw_record_tail_no_exact COMMAND agc_driver_buffer_raw_record_tail_tests)
    set_tests_properties(agc_driver_buffer_raw_record_tail_no_exact PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_EXACT_ROBUST_LOADS=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_buffer_raw_record_tail_no_exact PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
