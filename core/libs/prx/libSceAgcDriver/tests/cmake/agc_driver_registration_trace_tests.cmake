if(BUILD_TESTING)
    set_tests_properties(agc_driver_registration_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" TIMEOUT 30)
    set_property(TEST agc_driver_registration_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_registration_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_registration_trace_tests> disabled)
    set_tests_properties(agc_driver_registration_trace_disabled PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_registration_trace_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
