if(BUILD_TESTING)
    add_test(NAME agc_driver_two_device_pending_store_label COMMAND agc_driver_two_device_pending_store_tests --label)
    set_tests_properties(agc_driver_two_device_pending_store_label PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_STORE_AT_LABEL=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_two_device_pending_store_label PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
