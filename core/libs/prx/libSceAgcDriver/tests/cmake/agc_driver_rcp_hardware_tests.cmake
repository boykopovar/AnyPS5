if(BUILD_TESTING)
    set_target_properties(agc_driver_rcp_hardware_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_rcp_hardware_exact COMMAND agc_driver_rcp_hardware_tests)
    set_tests_properties(agc_driver_rcp_hardware_exact PROPERTIES ENVIRONMENT "APS5_EXACT_RCP=1;ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_rcp_hardware_exact PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
