if(BUILD_TESTING)
    foreach(mode point linear)
        add_test(NAME agc_driver_image_sample_skip_degamma_${mode} COMMAND agc_driver_image_sample_force_degamma_tests --skip-${mode})
        set_tests_properties(agc_driver_image_sample_skip_degamma_${mode} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_driver_image_sample_skip_degamma_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
