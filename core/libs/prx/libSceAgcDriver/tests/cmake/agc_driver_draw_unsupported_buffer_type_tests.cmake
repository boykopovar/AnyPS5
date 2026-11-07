if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_unsupported_buffer_type_skip_refused COMMAND agc_driver_draw_unsupported_buffer_type_tests --skip-refused)
    set_tests_properties(agc_driver_draw_unsupported_buffer_type_skip_refused PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SKIP_REFUSED_DRAWS=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_unsupported_buffer_type_skip_refused PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_draw_pipeline_test(agc_driver_draw_unsupported_buffer_type)
endif()
