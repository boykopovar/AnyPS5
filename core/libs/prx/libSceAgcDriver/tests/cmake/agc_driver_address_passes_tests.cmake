if(BUILD_TESTING)
    add_test(NAME agc_driver_address_passes_targets_split COMMAND agc_driver_address_passes_tests --targets)
    set_tests_properties(agc_driver_address_passes_targets_split PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_READ_PASSES=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_address_passes_targets_split PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_address_passes_draw_transitions COMMAND agc_driver_address_passes_tests --targets)
    set_tests_properties(agc_driver_address_passes_draw_transitions PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_READ_PASSES=1;APS5_DRAW_TRANSITIONS=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_address_passes_draw_transitions PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
