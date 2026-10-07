add_library(libSceWebBrowserDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceWebBrowserDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceWebBrowserDialog)

if(BUILD_TESTING)
    add_test_executable(guest_web_browser_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebBrowserDialog.cpp)
    target_include_directories(guest_web_browser_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_web_browser_dialog_tests PRIVATE libSceWebBrowserDialog)
    add_test(NAME guest_web_browser_dialog COMMAND guest_web_browser_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_web_browser_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceWebBrowserDialog>")
    endif()
endif()
