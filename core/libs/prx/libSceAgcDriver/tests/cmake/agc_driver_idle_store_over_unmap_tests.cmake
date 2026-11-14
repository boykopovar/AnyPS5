if(BUILD_TESTING)
    add_test(NAME agc_driver_idle_store_over_unmap_pipelined_dispatch COMMAND agc_driver_idle_store_over_unmap_tests)
    set_tests_properties(agc_driver_idle_store_over_unmap_pipelined_dispatch PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINE_DISPATCH=1;APS5_PIPELINE_DISPATCH_MISSES=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_idle_store_over_unmap_pipelined_dispatch PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
