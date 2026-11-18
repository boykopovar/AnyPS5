if(BUILD_TESTING)
    add_test(NAME agc_driver_store_idle_results_wake COMMAND agc_driver_store_idle_results_tests --idle-epoch-wake)
    set_tests_properties(agc_driver_store_idle_results_wake PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_store_idle_results_disabled COMMAND agc_driver_store_idle_results_tests --disabled-idle-store)
    set_tests_properties(agc_driver_store_idle_results_disabled PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_IDLE_STORE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_store_idle_results_wake PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_store_idle_results_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
