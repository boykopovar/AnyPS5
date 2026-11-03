if(BUILD_TESTING)
    add_test(NAME agc_driver_period_copy_no_hle COMMAND agc_driver_period_copy_tests)
    set_tests_properties(agc_driver_period_copy_no_hle PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_COPY_HLE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_period_copy_no_hle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
