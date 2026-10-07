add_library(libSceLoginDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceLoginDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceLoginDialog)

if(BUILD_TESTING)
    add_test_executable(guest_login_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestLoginDialog.cpp)
    target_include_directories(guest_login_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_login_dialog_tests PRIVATE libSceLoginDialog)
    add_test(NAME guest_login_dialog COMMAND guest_login_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_login_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLoginDialog>")
    endif()
endif()
