if(BUILD_TESTING)
    add_test(NAME agc_driver_mesh_vector_window_writes COMMAND agc_driver_mesh_vector_window_waves_tests --relative-writes-only)
    set_tests_properties(agc_driver_mesh_vector_window_writes PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1"
            SKIP_RETURN_CODE 77 TIMEOUT 30 FAIL_REGULAR_EXPRESSION "VUID-|SYNC-HAZARD-")
    if(WIN32)
        set_tests_properties(agc_driver_mesh_vector_window_writes PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
