add_library(libSceVrSetupDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceVrSetupDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVrSetupDialog)

if(BUILD_TESTING)
    add_test_executable(guest_vr_setup_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestVrSetupDialog.cpp)
    target_include_directories(guest_vr_setup_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vr_setup_dialog_tests PRIVATE libSceVrSetupDialog)
    add_test(NAME guest_vr_setup_dialog COMMAND guest_vr_setup_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_vr_setup_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVrSetupDialog>")
    endif()
endif()
