if(BUILD_TESTING)
    add_test(NAME agc_driver_stencil_replay_no_gpl COMMAND agc_driver_stencil_replay_tests)
    set_tests_properties(agc_driver_stencil_replay_no_gpl PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPL=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_stencil_replay_no_gpl PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
