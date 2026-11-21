if(BUILD_TESTING)
    set(aprNativeReadRun ${CMAKE_CURRENT_BINARY_DIR}/apr_read_native)
    file(MAKE_DIRECTORY ${aprNativeReadRun})
    add_test(NAME guest_apr_read_native COMMAND guest_apr_read_tests native
        WORKING_DIRECTORY ${aprNativeReadRun})
    set_tests_properties(guest_apr_read_native PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_apr_read_native PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
endif()
