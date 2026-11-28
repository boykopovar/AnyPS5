add_test_executable(agc_driver_wait_timeout_report_tests EXCLUDE_FROM_ALL tests/WaitTimeoutReport.cpp)
target_include_directories(agc_driver_wait_timeout_report_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_driver_wait_timeout_report_tests PRIVATE libSceAgcDriver libkernel libc)
set_target_properties(agc_driver_wait_timeout_report_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_wait_timeout_report_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_wait_timeout_report_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_wait_timeout_report COMMAND agc_driver_wait_timeout_report_tests)
    set_tests_properties(agc_driver_wait_timeout_report PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_WAIT_TIMEOUT_MS=20"
            TIMEOUT 10 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_wait_timeout_report PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
