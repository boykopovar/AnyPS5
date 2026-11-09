add_library(libScePs2EmuMenuDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libScePs2EmuMenuDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePs2EmuMenuDialog)

if(BUILD_TESTING)
    add_test_executable(guest_ps2_emu_menu_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestPs2EmuMenuDialog.cpp)
    target_include_directories(guest_ps2_emu_menu_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ps2_emu_menu_dialog_tests PRIVATE libScePs2EmuMenuDialog)
    add_test(NAME guest_ps2_emu_menu_dialog COMMAND guest_ps2_emu_menu_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_ps2_emu_menu_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePs2EmuMenuDialog>")
    endif()
endif()
