if(BUILD_TESTING)
    add_test_executable(agc_guest_read_known_zero_tests
            tests/GuestReadKnownZero.cpp
            Execution/src/GuestMemory.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_guest_read_known_zero_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_guest_read_known_zero_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_guest_read_known_zero_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_guest_read_known_zero_tests)
    add_test(NAME agc_guest_read_known_zero COMMAND agc_guest_read_known_zero_tests)
    set_tests_properties(agc_guest_read_known_zero PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_guest_read_known_zero PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
