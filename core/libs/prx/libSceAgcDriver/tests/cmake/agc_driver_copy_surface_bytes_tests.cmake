if(BUILD_TESTING)
    set_tests_properties(agc_driver_copy_surface_bytes PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SUSPEND_DRAIN=1")
    set_property(TEST agc_driver_copy_surface_bytes APPEND PROPERTY ENVIRONMENT_MODIFICATION "APS5_NO_COPY_ALIAS=unset:")
    add_test(NAME agc_driver_copy_surface_bytes_no_alias COMMAND agc_driver_copy_surface_bytes_tests)
    set_tests_properties(agc_driver_copy_surface_bytes_no_alias PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SUSPEND_DRAIN=1;APS5_NO_COPY_ALIAS=1")
    if(WIN32)
        set_tests_properties(agc_driver_copy_surface_bytes_no_alias PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
