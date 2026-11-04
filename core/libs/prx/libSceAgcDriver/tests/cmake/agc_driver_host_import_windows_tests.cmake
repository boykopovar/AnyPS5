if(BUILD_TESTING)
    if(WIN32)
        add_test(NAME agc_driver_host_import_alias_failure COMMAND agc_driver_host_import_windows_tests --alias-failure)
        set_tests_properties(agc_driver_host_import_alias_failure PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_driver_partial_resident_alias COMMAND agc_driver_host_import_windows_tests --partial-resident-alias)
        set_tests_properties(agc_driver_partial_resident_alias PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
