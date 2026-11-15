if(BUILD_TESTING)
    add_test(NAME agc_driver_tessellation_depth_range_remap COMMAND agc_driver_tessellation_tests --depth-remap)
    set_tests_properties(agc_driver_tessellation_depth_range_remap PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_tessellation_depth_range_remap PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
