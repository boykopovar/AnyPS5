
if(BUILD_TESTING)
    add_test(NAME agc_driver_depth_storage_copies COMMAND agc_driver_depth_storage_tests)
    add_test(NAME agc_driver_depth_storage_restricted_depth COMMAND agc_driver_depth_storage_tests --restricted-depth)
    set_tests_properties(agc_driver_depth_storage_copies PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DEPTH_TRANSFER_PASS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_depth_storage_restricted_depth PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_storage_copies agc_driver_depth_storage_restricted_depth PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
