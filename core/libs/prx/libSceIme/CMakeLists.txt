add_library(libSceIme SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceIme PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceIme)

if(BUILD_TESTING)
    add_test_executable(guest_ime_parameters_tests ${LIBS_INCLUDE_DIR}/tests/GuestImeParameters.cpp)
    target_include_directories(guest_ime_parameters_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ime_parameters_tests PRIVATE libSceIme libc)
    configure_windows_unwind(guest_ime_parameters_tests)
    add_test(NAME guest_ime_parameters COMMAND guest_ime_parameters_tests)
    if(WIN32)
        set_tests_properties(guest_ime_parameters PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()

    add_test_executable(guest_ime_pad_route_tests ${LIBS_INCLUDE_DIR}/tests/GuestImePadRoute.cpp)
    target_include_directories(guest_ime_pad_route_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ime_pad_route_tests PRIVATE libSceIme libc)
    configure_windows_unwind(guest_ime_pad_route_tests)
    add_test(NAME guest_ime_pad_route COMMAND guest_ime_pad_route_tests)
    if(WIN32)
        set_tests_properties(guest_ime_pad_route PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()

    add_test_executable(guest_ime_keyboard_tests ${LIBS_INCLUDE_DIR}/tests/GuestImeKeyboard.cpp)
    target_include_directories(guest_ime_keyboard_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ime_keyboard_tests PRIVATE libSceIme libc)
    configure_windows_unwind(guest_ime_keyboard_tests)
    add_test(NAME guest_ime_keyboard COMMAND guest_ime_keyboard_tests)
    if(WIN32)
        set_tests_properties(guest_ime_keyboard PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()

    add_test_executable(guest_ime_filter_text_tests ${LIBS_INCLUDE_DIR}/tests/GuestImeFilterText.cpp)
    target_include_directories(guest_ime_filter_text_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ime_filter_text_tests PRIVATE libSceIme libc)
    configure_windows_unwind(guest_ime_filter_text_tests)
    add_test(NAME guest_ime_filter_text COMMAND guest_ime_filter_text_tests)
    if(WIN32)
        set_tests_properties(guest_ime_filter_text PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()

    add_test_executable(guest_ime_open_tests ${LIBS_INCLUDE_DIR}/tests/GuestImeOpen.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/ImeTextInput.cpp)
    target_include_directories(guest_ime_open_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(guest_ime_open_tests PRIVATE libSceIme libc)
    configure_windows_unwind(guest_ime_open_tests)
    add_test(NAME guest_ime_open COMMAND guest_ime_open_tests)
    if(WIN32)
        set_tests_properties(guest_ime_open PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()
endif()
