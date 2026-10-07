add_library(libSceErrorDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceErrorDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceErrorDialog)

if(BUILD_TESTING)
    add_test_executable(guest_error_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestErrorDialog.cpp)
    target_include_directories(guest_error_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_error_dialog_tests PRIVATE libSceErrorDialog)
    add_test(NAME guest_error_dialog COMMAND guest_error_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_error_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceErrorDialog>")
    endif()
endif()
