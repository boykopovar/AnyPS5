if(BUILD_TESTING)
    foreach(agcInputTwinMode IN ITEMS off unstaged)
        add_test(NAME agc_driver_input_twins_${agcInputTwinMode} COMMAND agc_driver_input_twins_tests --${agcInputTwinMode})
        set_tests_properties(agc_driver_input_twins_${agcInputTwinMode} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_input_twins_${agcInputTwinMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_input_twins_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_INPUT_TWINS=1")
endif()
