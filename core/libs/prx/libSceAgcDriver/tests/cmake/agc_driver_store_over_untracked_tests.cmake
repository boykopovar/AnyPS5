if(BUILD_TESTING)
    add_test(NAME agc_driver_store_pending_identity COMMAND agc_driver_store_over_untracked_tests --pending-identity)
    set_tests_properties(agc_driver_store_pending_identity PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_store_pending_identity PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
