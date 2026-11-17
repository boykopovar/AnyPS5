if(BUILD_TESTING)
    add_test(NAME agc_submit_progress COMMAND agc_driver_submit_progress_tests)
    set_tests_properties(agc_submit_progress PROPERTIES
            ENVIRONMENT "APS5_SUBMIT_PROGRESS=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_submit_progress PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_submit_progress_lean_front COMMAND agc_driver_submit_progress_tests)
    set_tests_properties(agc_submit_progress_lean_front PROPERTIES
            ENVIRONMENT "APS5_SUBMIT_PROGRESS=1;APS5_LEAN_FRONT=1;APS5_VERIFY_LEAN_FRONT=2" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            FAIL_REGULAR_EXPRESSION "${agcLeanFrontDifference}")
    if(WIN32)
        set_tests_properties(agc_submit_progress_lean_front PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_submit_progress_no_timeline COMMAND agc_driver_submit_progress_tests --fallback-only)
    set_tests_properties(agc_submit_progress_no_timeline PROPERTIES
            ENVIRONMENT "APS5_SUBMIT_PROGRESS=1;APS5_NO_TIMELINE=1"
            TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_submit_progress_no_timeline PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
