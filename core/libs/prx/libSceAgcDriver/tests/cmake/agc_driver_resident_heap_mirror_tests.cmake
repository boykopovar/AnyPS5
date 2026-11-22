if(BUILD_TESTING)
    add_test(NAME agc_driver_resident_heap_mirror_no_publish COMMAND agc_driver_resident_heap_mirror_tests)
    set_tests_properties(agc_driver_resident_heap_mirror_no_publish PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_MIRROR_PUBLISH=1;ANYPS5_TEST_EXPECT_MIRROR_PUBLISH=1" SKIP_RETURN_CODE 77 PASS_REGULAR_EXPRESSION "instead of publishing it into the heap mirror" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_resident_heap_mirror_no_publish PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
