if(BUILD_TESTING)
    foreach(agcInputTwinMode IN ITEMS off unstaged)
        add_test(NAME agc_driver_input_twins_${agcInputTwinMode} COMMAND agc_driver_input_twins_tests --${agcInputTwinMode})
    endforeach()
    add_test(NAME agc_driver_input_twins_verify COMMAND agc_driver_input_twins_tests)
    add_test(NAME agc_driver_input_twins_unstaged_verify COMMAND agc_driver_input_twins_tests --unstaged)
    add_test(NAME agc_driver_input_twins_unproved COMMAND agc_driver_input_twins_tests)
    set(agcInputTwinTests agc_driver_input_twins_off agc_driver_input_twins_unstaged agc_driver_input_twins_verify agc_driver_input_twins_unstaged_verify agc_driver_input_twins_unproved)
    set_tests_properties(${agcInputTwinTests} PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(${agcInputTwinTests} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_driver_input_twins_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_INPUT_TWINS=1")
    set_tests_properties(agc_driver_input_twins_verify agc_driver_input_twins_unstaged_verify PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PROOFS=1" FAIL_REGULAR_EXPRESSION "\\[draw-proofs\\] verify")
    set_tests_properties(agc_driver_input_twins_unproved PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_PROOFS=1")
endif()
