if(BUILD_TESTING)
    add_test(NAME agc_driver_linear_readback_label_off COMMAND agc_driver_linear_readback_label_tests)
    set_tests_properties(agc_driver_linear_readback_label_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_LABEL_READBACK_STORE=1" SKIP_RETURN_CODE 77 PASS_REGULAR_EXPRESSION "read after the label" TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_linear_readback_label_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
