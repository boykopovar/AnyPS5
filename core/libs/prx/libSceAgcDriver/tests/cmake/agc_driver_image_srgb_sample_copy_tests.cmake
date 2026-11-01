if(BUILD_TESTING)
    add_test(NAME agc_driver_image_srgb_sample_copy_forced COMMAND agc_driver_image_srgb_sample_copy_tests)
    set_tests_properties(agc_driver_image_srgb_sample_copy_forced PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SRGB_SHADER_DECODE=copy"
            SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_image_srgb_sample_copy_forced PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
