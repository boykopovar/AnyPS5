if(BUILD_TESTING AND WIN32)
    add_test_executable(guest_socket_pair_rights_tests ${LIBS_INCLUDE_DIR}/tests/GuestSocketPairRights.cpp)
    target_include_directories(guest_socket_pair_rights_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_socket_pair_rights_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_socket_pair_rights_tests)
    add_test(NAME guest_socket_pair_rights COMMAND guest_socket_pair_rights_tests)
    set_tests_properties(guest_socket_pair_rights PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
endif()
