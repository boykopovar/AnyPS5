if(BUILD_TESTING AND WIN32)
    add_test_executable(guest_file_lock_queued_upgrade_tests ${LIBS_INCLUDE_DIR}/tests/GuestFileLockQueuedUpgrade.cpp)
    target_include_directories(guest_file_lock_queued_upgrade_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_file_lock_queued_upgrade_tests PRIVATE libkernel libc)
    configure_windows_unwind(guest_file_lock_queued_upgrade_tests)
    add_test(NAME guest_file_lock_queued_upgrade COMMAND guest_file_lock_queued_upgrade_tests)
    set_tests_properties(guest_file_lock_queued_upgrade PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
endif()
