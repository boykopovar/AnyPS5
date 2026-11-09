if(BUILD_TESTING)
    add_test(NAME agc_driver_emulated_color_compare_srgb_decode COMMAND agc_driver_emulated_color_compare_tests)
    set_tests_properties(agc_driver_emulated_color_compare_srgb_decode PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SRGB_SHADER_DECODE=1"
            SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_emulated_color_compare agc_driver_emulated_color_compare_srgb_decode PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_emulated_color_compare_srgb_decode PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
