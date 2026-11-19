if(BUILD_TESTING)
    add_test(NAME agc_driver_vertex_table_read_order_counts_off COMMAND agc_driver_vertex_table_read_order_tests)
    set_tests_properties(agc_driver_vertex_table_read_order_counts_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DECODE_WATCH_COUNTS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_vertex_table_read_order_counts_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
