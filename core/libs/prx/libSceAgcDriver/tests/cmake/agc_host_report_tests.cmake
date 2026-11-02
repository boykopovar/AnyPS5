add_test_executable(agc_host_report_tests EXCLUDE_FROM_ALL
        tests/HostReport.cpp
        Execution/src/HostReport.cpp
)
target_include_directories(agc_host_report_tests PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(agc_host_report_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_host_report_tests)

if(BUILD_TESTING)
    set_target_properties(agc_host_report_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_host_report COMMAND agc_host_report_tests)
    set_tests_properties(agc_host_report PROPERTIES TIMEOUT 20)
    if(MINGW)
        target_link_libraries(agc_host_report_tests PRIVATE libc)
        set_tests_properties(agc_host_report PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
