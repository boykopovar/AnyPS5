if(BUILD_TESTING AND TARGET agc_driver_last_fragment_outputs_tests)
    add_test(NAME agc_driver_last_fragment_outputs_off COMMAND agc_driver_last_fragment_outputs_tests)
    set_tests_properties(agc_driver_last_fragment_outputs_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_LAST_FRAGMENT_OUTPUTS=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_last_fragment_outputs_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
