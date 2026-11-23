if(BUILD_TESTING AND WIN32)
    add_test_executable(guest_windows_permission_bits_tests ${LIBS_INCLUDE_DIR}/tests/GuestWindowsPermissionBits.cpp)
    target_include_directories(guest_windows_permission_bits_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_windows_permission_bits_tests PRIVATE libkernel_sys libkernel libc)
    configure_windows_unwind(guest_windows_permission_bits_tests)
    add_test(NAME guest_windows_permission_bits COMMAND guest_windows_permission_bits_tests)
    set_tests_properties(guest_windows_permission_bits PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
endif()
