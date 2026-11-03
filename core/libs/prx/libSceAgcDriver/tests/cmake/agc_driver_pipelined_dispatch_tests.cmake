if(BUILD_TESTING)
    add_test(NAME agc_driver_pipelined_dispatch_item COMMAND agc_driver_pipelined_dispatch_tests)
    set_tests_properties(agc_driver_pipelined_dispatch_item PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINE_DISPATCH=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pipelined_dispatch_item PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
