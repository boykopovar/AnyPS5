target_link_libraries(agc_driver_linear_target_completion_points_tests PRIVATE libkernel)

if(BUILD_TESTING)
    add_test(NAME agc_driver_linear_target_completion_points_sync_flip COMMAND agc_driver_linear_target_completion_points_tests --flip)
    set_tests_properties(agc_driver_linear_target_completion_points_sync_flip PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SYNC_FLIP=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_linear_target_completion_points_unpipelined COMMAND agc_driver_linear_target_completion_points_tests --label --interrupt)
    set_tests_properties(agc_driver_linear_target_completion_points_unpipelined PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_linear_target_completion_points_sync_flip agc_driver_linear_target_completion_points_unpipelined PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
