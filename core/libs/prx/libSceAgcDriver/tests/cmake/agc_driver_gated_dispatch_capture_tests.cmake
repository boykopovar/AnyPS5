if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_gated_dispatch_capture)
    add_test(NAME agc_driver_gated_dispatch_capture_short_timeout COMMAND agc_driver_gated_dispatch_capture_tests)
    set_tests_properties(agc_driver_gated_dispatch_capture_short_timeout PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_WAIT_TIMEOUT_MS=1;APS5_NO_PIPELINE_LTO=1"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_gated_dispatch_capture_short_timeout PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
