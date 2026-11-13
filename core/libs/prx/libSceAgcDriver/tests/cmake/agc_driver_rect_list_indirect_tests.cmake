if(BUILD_TESTING)
    add_test(NAME agc_driver_rect_list_indirect_items COMMAND agc_driver_rect_list_indirect_tests)
    set_tests_properties(agc_driver_rect_list_indirect_items PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=1;APS5_PIPELINE_INDIRECT=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_rect_list_indirect_items PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
