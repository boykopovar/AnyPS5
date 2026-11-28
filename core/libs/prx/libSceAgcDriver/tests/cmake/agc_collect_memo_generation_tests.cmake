if(BUILD_TESTING)
    add_test_executable(agc_collect_memo_generation_tests
            tests/CollectMemoGeneration.cpp
            Execution/src/GuestMemory.cpp
            Graphics/src/RuntimeSwitches.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_collect_memo_generation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_collect_memo_generation_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_collect_memo_generation_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_collect_memo_generation_tests)
    add_test(NAME agc_collect_memo_generation COMMAND agc_collect_memo_generation_tests)
    add_test(NAME agc_collect_coverage_interior COMMAND agc_collect_memo_generation_tests --interior)
    add_test(NAME agc_collect_coverage_unions COMMAND agc_collect_memo_generation_tests --unions)
    add_test(NAME agc_collect_coverage_invalidation COMMAND agc_collect_memo_generation_tests --invalidation)
    set(collect_memo_tests agc_collect_memo_generation agc_collect_coverage_interior agc_collect_coverage_unions agc_collect_coverage_invalidation)
    set_tests_properties(${collect_memo_tests} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(${collect_memo_tests} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
