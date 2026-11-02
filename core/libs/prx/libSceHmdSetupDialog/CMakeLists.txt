add_library(libSceHmdSetupDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceHmdSetupDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceHmdSetupDialog)

if(BUILD_TESTING)
    add_test_executable(guest_hmd_setup_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestHmdSetupDialog.cpp)
    target_include_directories(guest_hmd_setup_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_hmd_setup_dialog_tests PRIVATE libSceHmdSetupDialog)
    add_test(NAME guest_hmd_setup_dialog COMMAND guest_hmd_setup_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_hmd_setup_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceHmdSetupDialog>")
    endif()
endif()
