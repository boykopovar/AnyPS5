if(BUILD_TESTING)
    add_test(NAME agc_driver_list_restart_compacted COMMAND agc_driver_list_restart_tests --compacted)
    set_tests_properties(agc_driver_list_restart_compacted PROPERTIES ENVIRONMENT "APS5_NO_LIST_RESTART=1;APS5_POISON_VERTEX_PREFIX=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_list_restart_compacted PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
