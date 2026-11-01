if(BUILD_TESTING)
    add_test(NAME agc_driver_indirect_thread_dimensions_cpu COMMAND agc_driver_indirect_thread_dimensions_tests)
    set_tests_properties(agc_driver_indirect_thread_dimensions_cpu PROPERTIES ENVIRONMENT "APS5_NO_GPU_THREAD_DIMENSIONS=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_indirect_thread_dimensions_cpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
