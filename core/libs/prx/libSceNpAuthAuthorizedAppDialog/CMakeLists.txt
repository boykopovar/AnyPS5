add_library(libSceNpAuthAuthorizedAppDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_link_libraries(libSceNpAuthAuthorizedAppDialog PRIVATE libSceIme)
target_include_directories(libSceNpAuthAuthorizedAppDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpAuthAuthorizedAppDialog)

target_link_libraries(libSceNpAuthAuthorizedAppDialog PRIVATE libSceUserService)

if(BUILD_TESTING)
    add_test_executable(guest_np_auth_authorized_app_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpAuthAuthorizedAppDialog.cpp)
    target_include_directories(guest_np_auth_authorized_app_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_auth_authorized_app_dialog_tests PRIVATE libSceNpAuthAuthorizedAppDialog libSceIme libSceUserService)
    add_test(NAME guest_np_auth_authorized_app_dialog COMMAND guest_np_auth_authorized_app_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_np_auth_authorized_app_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpAuthAuthorizedAppDialog>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()
endif()
