if(WIN32)
    add_test_executable(guest_copy_out_arena_windows_tests ${LIBS_INCLUDE_DIR}/tests/GuestCopyOutArenaWindows.cpp)
    target_include_directories(guest_copy_out_arena_windows_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_copy_out_arena_windows_tests PRIVATE libc)
    configure_windows_unwind(guest_copy_out_arena_windows_tests)
    add_test(NAME guest_copy_out_arena_windows COMMAND guest_copy_out_arena_windows_tests)
    set_tests_properties(guest_copy_out_arena_windows PROPERTIES TIMEOUT 20 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()
