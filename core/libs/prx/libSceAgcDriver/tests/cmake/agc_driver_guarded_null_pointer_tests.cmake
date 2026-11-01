if(BUILD_TESTING)
    add_test(NAME agc_driver_guarded_direct_root COMMAND agc_driver_guarded_null_pointer_tests --direct-root-only)
    set_tests_properties(agc_driver_guarded_direct_root PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_guarded_direct_root PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
