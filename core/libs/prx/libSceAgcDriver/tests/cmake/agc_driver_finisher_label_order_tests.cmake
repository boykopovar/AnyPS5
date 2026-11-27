if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_finisher_label_order)
    add_test(NAME agc_driver_finisher_label_order_profiled COMMAND agc_driver_finisher_label_order_tests)
    set_tests_properties(agc_driver_finisher_label_order_profiled PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_PROFILE_DRAW=1"
            PASS_REGULAR_EXPRESSION "queued through the finisher, draw phases reported once per draw"
            TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_finisher_label_order_profiled PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
