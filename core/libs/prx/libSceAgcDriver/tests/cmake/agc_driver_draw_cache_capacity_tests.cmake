if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_cache_metadata_budget COMMAND agc_driver_draw_cache_capacity_tests --metadata-budget)
    add_test(NAME agc_driver_draw_cache_shared_result COMMAND agc_driver_draw_cache_capacity_tests --shared-result)
    add_test(NAME agc_driver_draw_cache_decode_budget COMMAND agc_driver_draw_cache_capacity_tests --decode-budget)
    set_tests_properties(agc_driver_draw_cache_metadata_budget PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_CACHE_BYTES=10485760;APS5_NO_RESULT_MEMO=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_draw_cache_shared_result PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_CACHE_ENTRIES=2" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_draw_cache_decode_budget PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_RESULT_MEMO=1;APS5_DRAW_CACHE_BYTES=19500000" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_cache_metadata_budget agc_driver_draw_cache_shared_result agc_driver_draw_cache_decode_budget PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
