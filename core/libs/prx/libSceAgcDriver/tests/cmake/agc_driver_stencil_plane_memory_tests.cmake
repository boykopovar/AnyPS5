if(BUILD_TESTING)
    add_test(NAME agc_driver_stencil_plane_memory_no_write_watch COMMAND agc_driver_stencil_plane_memory_tests --rewatch-only)
    set_tests_properties(agc_driver_stencil_plane_memory_no_write_watch PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1;APS5_NO_WRITE_WATCH=1"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_stencil_plane_memory_no_write_watch PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
