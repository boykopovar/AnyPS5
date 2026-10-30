add_library(libScePad SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PadState.cpp
)
target_include_directories(libScePad PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePad PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePad)

if(BUILD_TESTING)
    add_test_executable(guest_pad_tests ${LIBS_INCLUDE_DIR}/tests/GuestPad.cpp)
    target_include_directories(guest_pad_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(guest_pad_tests PRIVATE libScePad libc)
    configure_windows_unwind(guest_pad_tests)
    add_test(NAME guest_pad COMMAND guest_pad_tests)
    if(WIN32)
        set_tests_properties(guest_pad PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePad>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_pad_vibration_mode_tests ${LIBS_INCLUDE_DIR}/tests/GuestPadVibrationMode.cpp)
    target_include_directories(guest_pad_vibration_mode_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pad_vibration_mode_tests PRIVATE libScePad libc)
    configure_windows_unwind(guest_pad_vibration_mode_tests)
    add_test(NAME guest_pad_vibration_mode COMMAND guest_pad_vibration_mode_tests)
    if(WIN32)
        set_tests_properties(guest_pad_vibration_mode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePad>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(pad_rumble_intensity_tests tests/PadRumbleIntensity.cpp)
    target_include_directories(pad_rumble_intensity_tests PRIVATE ${LIBS_INCLUDE_DIR})
    configure_host_test_executable(pad_rumble_intensity_tests)
    add_test(NAME pad_rumble_intensity COMMAND pad_rumble_intensity_tests)
    set_tests_properties(pad_rumble_intensity PROPERTIES TIMEOUT 30)
endif()
