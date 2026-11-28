if(BUILD_TESTING)
    add_test(NAME agc_driver_image_by_extra_base COMMAND agc_driver_image_by_tests --extra-base)
    set_tests_properties(agc_driver_image_by_extra_base PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_image_by_extra_base PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
