if(TARGET agc_driver_memset_exclusive_fill_tests)
    target_link_libraries(agc_driver_memset_exclusive_fill_tests PRIVATE libSceAgc)

    if(BUILD_TESTING)
        add_test(NAME agc_driver_memset_exclusive_fill_translated COMMAND agc_driver_memset_exclusive_fill_tests)
        set_tests_properties(agc_driver_memset_exclusive_fill_translated PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_FILL_HLE=1"
            SKIP_RETURN_CODE 77
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 60)
        if(WIN32)
            set_tests_properties(agc_driver_memset_exclusive_fill_translated PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
endif()
