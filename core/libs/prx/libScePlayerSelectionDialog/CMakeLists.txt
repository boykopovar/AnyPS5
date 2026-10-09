add_library(libScePlayerSelectionDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libScePlayerSelectionDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePlayerSelectionDialog)

if(BUILD_TESTING)
    add_test_executable(guest_player_selection_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestPlayerSelectionDialog.cpp)
    target_include_directories(guest_player_selection_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_player_selection_dialog_tests PRIVATE libScePlayerSelectionDialog)
    add_test(NAME guest_player_selection_dialog COMMAND guest_player_selection_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_player_selection_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePlayerSelectionDialog>")
    endif()
endif()
