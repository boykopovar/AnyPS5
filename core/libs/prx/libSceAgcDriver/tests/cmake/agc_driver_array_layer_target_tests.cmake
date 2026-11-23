if(BUILD_TESTING)
    add_test(NAME agc_driver_array_layer_target_cpu COMMAND agc_driver_array_layer_target_tests)
    set_tests_properties(agc_driver_array_layer_target_cpu PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_HOST_IMPORT=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_array_layer_borrow COMMAND agc_driver_array_layer_target_tests --layer-borrow)
    set_tests_properties(agc_driver_array_layer_borrow PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_array_layer_borrow_off COMMAND agc_driver_array_layer_target_tests --layer-borrow)
    set_tests_properties(agc_driver_array_layer_borrow_off PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_ALIAS_BORROW=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_array_layer_target_cpu agc_driver_array_layer_borrow agc_driver_array_layer_borrow_off PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
