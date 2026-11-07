if(BUILD_TESTING)
    add_test(NAME agc_driver_gds_append_consume_copied COMMAND agc_driver_gds_append_consume_tests)
    set_tests_properties(agc_driver_gds_append_consume_copied PROPERTIES TIMEOUT 60 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_HOST_IMPORT=1;APS5_NO_SHARED_GDS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_gds_append_consume_copied PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
