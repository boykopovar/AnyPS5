add_library(libSceSocialScreenDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSocialScreenDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSocialScreenDialog)

if(BUILD_TESTING)
    add_test_executable(guest_social_screen_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestSocialScreenDialog.cpp)
    target_include_directories(guest_social_screen_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_social_screen_dialog_tests PRIVATE libSceSocialScreenDialog)
    add_test(NAME guest_social_screen_dialog COMMAND guest_social_screen_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_social_screen_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSocialScreenDialog>")
    endif()
endif()
