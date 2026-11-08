if(BUILD_TESTING)
    add_test_executable(agc_collect_memo_generation_tests
            tests/CollectMemoGeneration.cpp
            Execution/src/GuestMemory.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_collect_memo_generation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_collect_memo_generation_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_collect_memo_generation_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_collect_memo_generation_tests)
    add_test(NAME agc_collect_memo_generation COMMAND agc_collect_memo_generation_tests)
    set_tests_properties(agc_collect_memo_generation PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_collect_memo_generation PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
