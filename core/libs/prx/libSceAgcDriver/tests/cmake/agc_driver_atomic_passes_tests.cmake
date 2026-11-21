if(BUILD_TESTING)
    add_test(NAME agc_driver_atomic_passes_split COMMAND agc_driver_atomic_passes_tests)
    set_tests_properties(agc_driver_atomic_passes_split PROPERTIES ENVIRONMENT "APS5_NO_ATOMIC_PASSES=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_atomic_passes_split PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
