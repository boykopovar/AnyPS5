if(BUILD_TESTING AND NOT WIN32)
    add_test(NAME agc_driver_memo_pending_collect_compared COMMAND agc_driver_memo_pending_collect_tests)
    set_tests_properties(agc_driver_memo_pending_collect_compared PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_WRITE_WATCH_IMPORTS=unwatch" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
