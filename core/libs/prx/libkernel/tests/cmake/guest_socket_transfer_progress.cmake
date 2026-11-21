if(BUILD_TESTING AND WIN32)
    add_test_executable(guest_socket_transfer_progress_tests ${LIBS_INCLUDE_DIR}/tests/GuestSocketTransferProgress.cpp)
    target_include_directories(guest_socket_transfer_progress_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_socket_transfer_progress_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_socket_transfer_progress_tests)
    add_test(NAME guest_socket_transfer_progress COMMAND guest_socket_transfer_progress_tests)
    set_tests_properties(guest_socket_transfer_progress PROPERTIES TIMEOUT 20
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
endif()
