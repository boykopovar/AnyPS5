if(BUILD_TESTING AND NOT WIN32)
    add_test_executable(agc_collect_memo_generation_tests
            tests/CollectMemoGeneration.cpp
            Execution/src/GuestMemory.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_collect_memo_generation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_collect_memo_generation_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_collect_memo_generation_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME agc_collect_memo_generation COMMAND agc_collect_memo_generation_tests)
    set_tests_properties(agc_collect_memo_generation PROPERTIES TIMEOUT 30)
endif()
