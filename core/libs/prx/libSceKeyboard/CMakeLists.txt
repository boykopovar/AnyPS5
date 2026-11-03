add_library(libSceKeyboard SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/keyboard_impl.cpp
)
target_include_directories(libSceKeyboard PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceKeyboard PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceKeyboard)

if(BUILD_TESTING)
    add_test_executable(keyboard_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Keyboard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/KeyboardInput.cpp
    )
    target_include_directories(keyboard_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(keyboard_tests PRIVATE libSceKeyboard libSceIme libkernel libc)
    configure_windows_unwind(keyboard_tests)
    add_test(NAME keyboard_input COMMAND keyboard_tests)
    if(WIN32)
        set_tests_properties(keyboard_input PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceKeyboard>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()
    add_test_executable(keyboard_dialog_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/KeyboardDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/KeyboardInput.cpp
    )
    target_include_directories(keyboard_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(keyboard_dialog_tests PRIVATE libSceKeyboard libSceIme libkernel libc)
    configure_windows_unwind(keyboard_dialog_tests)
    add_test(NAME keyboard_dialog COMMAND keyboard_dialog_tests)
    if(WIN32)
        set_tests_properties(keyboard_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceKeyboard>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>")
    endif()
endif()
