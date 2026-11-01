add_library(libSceGameCustomDataDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceGameCustomDataDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceGameCustomDataDialog)

if(BUILD_TESTING)
    add_test_executable(guest_game_custom_data_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestGameCustomDataDialog.cpp)
    target_include_directories(guest_game_custom_data_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_game_custom_data_dialog_tests PRIVATE libSceGameCustomDataDialog)
    add_test(NAME guest_game_custom_data_dialog COMMAND guest_game_custom_data_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_game_custom_data_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGameCustomDataDialog>")
    endif()
endif()
