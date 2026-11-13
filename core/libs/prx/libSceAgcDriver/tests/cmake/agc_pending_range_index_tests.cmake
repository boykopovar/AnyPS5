if(BUILD_TESTING)
    add_test_executable(agc_pending_range_index_tests
            tests/PendingRangeIndex.cpp
    )
    target_include_directories(agc_pending_range_index_tests PRIVATE ${LIBS_INCLUDE_DIR})
    set_target_properties(agc_pending_range_index_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_pending_range_index_tests)
    add_test(NAME agc_pending_range_index COMMAND agc_pending_range_index_tests)
    set_tests_properties(agc_pending_range_index PROPERTIES TIMEOUT 30)
endif()
