if(BUILD_TESTING)
    add_test(NAME agc_driver_uint8x4_color_target_no_resident COMMAND agc_driver_uint8x4_color_target_tests)
    set_tests_properties(agc_driver_uint8x4_color_target_no_resident PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_RESIDENT_TARGETS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_uint8x4_color_target_no_resident PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
