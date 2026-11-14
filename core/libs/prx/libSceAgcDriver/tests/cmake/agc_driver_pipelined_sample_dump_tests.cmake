if(BUILD_TESTING)
    add_test(NAME agc_driver_pipelined_sample_dump_drained COMMAND agc_driver_pipelined_sample_dump_tests)
    set_tests_properties(agc_driver_pipelined_sample_dump_drained PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINE_DRAIN_SAMPLE_DUMPS=1"
            SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_pipelined_sample_dump_drained PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
