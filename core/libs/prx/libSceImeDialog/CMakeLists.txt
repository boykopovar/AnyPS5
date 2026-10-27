add_library(libSceImeDialog SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceImeDialog PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceImeDialog PRIVATE libSceIme)
configure_windows_unwind(libSceImeDialog)

if(BUILD_TESTING)
    add_test_executable(guest_ime_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestImeDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/ImeTextInput.cpp)
    target_include_directories(guest_ime_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(guest_ime_dialog_tests PRIVATE libSceImeDialog libSceIme libc)
    configure_windows_unwind(guest_ime_dialog_tests)
    add_test(NAME guest_ime_dialog COMMAND guest_ime_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_ime_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceImeDialog>")
    endif()
endif()
