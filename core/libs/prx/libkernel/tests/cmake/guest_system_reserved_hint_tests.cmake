if(BUILD_TESTING AND WIN32)
    add_test_executable(guest_system_reserved_hint_tests ${LIBS_INCLUDE_DIR}/tests/GuestSystemReservedHint.cpp)
    target_include_directories(guest_system_reserved_hint_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_system_reserved_hint_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_system_reserved_hint_tests)
    add_test(NAME guest_system_reserved_hint COMMAND guest_system_reserved_hint_tests)
    set_tests_properties(guest_system_reserved_hint PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
endif()
