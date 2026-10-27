if(BUILD_TESTING)
    set_tests_properties(agc_driver_image_memory PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_image_memory_no_slabs COMMAND agc_driver_image_memory_tests --no-sharing-check)
    set_tests_properties(agc_driver_image_memory_no_slabs PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_image_memory_no_slabs APPEND PROPERTY ENVIRONMENT "APS5_NO_IMAGE_SLABS=1")
    add_test(NAME agc_driver_image_memory_teardown_failure COMMAND agc_driver_image_memory_tests --teardown-failure)
    add_test(NAME agc_driver_image_memory_no_slabs_teardown_failure COMMAND agc_driver_image_memory_tests --no-sharing-check --teardown-failure)
    set_tests_properties(agc_driver_image_memory_teardown_failure agc_driver_image_memory_no_slabs_teardown_failure PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_image_memory_no_slabs_teardown_failure APPEND PROPERTY ENVIRONMENT "APS5_NO_IMAGE_SLABS=1")
    if(WIN32)
        set_tests_properties(agc_driver_image_memory_no_slabs agc_driver_image_memory_teardown_failure
                agc_driver_image_memory_no_slabs_teardown_failure PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
