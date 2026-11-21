if(BUILD_TESTING)
    add_test(NAME agc_driver_late_test_merge_split_counting_pixel COMMAND agc_driver_late_test_merge_split_tests --counting-pixel)
    add_test(NAME agc_driver_late_test_merge_split_counting_vertex COMMAND agc_driver_late_test_merge_split_tests --counting-vertex)
    add_test(NAME agc_driver_late_test_merge_split_depth_clear COMMAND agc_driver_late_test_merge_split_tests --depth-clear)
    set_tests_properties(agc_driver_late_test_merge_split_counting_pixel agc_driver_late_test_merge_split_counting_vertex agc_driver_late_test_merge_split_depth_clear PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_late_test_merge_split_counting_pixel agc_driver_late_test_merge_split_counting_vertex agc_driver_late_test_merge_split_depth_clear PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
