if(BUILD_TESTING)
    add_test(NAME agc_driver_overlap_staged_copies_inline COMMAND agc_driver_overlap_staged_copies_tests --inline)
    set_tests_properties(agc_driver_overlap_staged_copies_inline PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_BATCHED_STAGING=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_overlap_staged_copies_inline PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
