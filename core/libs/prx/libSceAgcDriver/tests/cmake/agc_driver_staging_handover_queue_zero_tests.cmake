if(BUILD_TESTING)
    add_test(NAME agc_driver_staging_handover_queue_zero_off COMMAND agc_driver_staging_handover_queue_zero_tests --off)
    set_tests_properties(agc_driver_staging_handover_queue_zero_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_STAGING_HANDOVER=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_staging_handover_queue_zero_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
