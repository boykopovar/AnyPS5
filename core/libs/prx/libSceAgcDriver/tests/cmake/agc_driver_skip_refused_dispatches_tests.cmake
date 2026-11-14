if(BUILD_TESTING)
    add_test(NAME agc_driver_skip_refused_dispatches_max_words COMMAND agc_driver_skip_refused_dispatches_tests max-words)
    set_tests_properties(agc_driver_skip_refused_dispatches_max_words PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_skip_refused_dispatches_max_words PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
