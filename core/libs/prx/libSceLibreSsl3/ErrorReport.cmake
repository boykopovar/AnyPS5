target_sources(libSceLibreSsl3 PRIVATE src/ErrorReport.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl3_error_report_tests tests/ErrorReport.cpp)
    target_include_directories(guest_libressl3_error_report_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl3_error_report_tests PRIVATE libSceLibreSsl3 libSceLibreSsl libkernel libc)
    configure_windows_unwind(guest_libressl3_error_report_tests)
    add_test(NAME guest_libressl3_error_report COMMAND guest_libressl3_error_report_tests)
    set_tests_properties(guest_libressl3_error_report PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_libressl3_error_report PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl3>")
    endif()
endif()
