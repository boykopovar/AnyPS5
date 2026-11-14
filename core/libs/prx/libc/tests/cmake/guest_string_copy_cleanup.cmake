if(BUILD_TESTING AND (MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    add_test_executable(guest_string_copy_cleanup_tests
        ${LIBS_INCLUDE_DIR}/tests/GuestStringCopyCleanup.cpp)
    target_include_directories(guest_string_copy_cleanup_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_string_copy_cleanup_tests PRIVATE libc)
    configure_windows_unwind(guest_string_copy_cleanup_tests)
    add_test(NAME guest_string_copy_cleanup COMMAND guest_string_copy_cleanup_tests)
    set_tests_properties(guest_string_copy_cleanup PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_string_copy_cleanup PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
