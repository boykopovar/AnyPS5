add_test_executable(agc_written_buffer_index_tests EXCLUDE_FROM_ALL
        tests/WrittenBufferIndex.cpp
        Execution/src/Driver/Memory/WrittenBufferIndex.cpp
)
target_include_directories(agc_written_buffer_index_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_written_buffer_index_tests PRIVATE libc)
set_target_properties(agc_written_buffer_index_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_written_buffer_index_tests)

if(BUILD_TESTING)
    set_target_properties(agc_written_buffer_index_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_written_buffer_index COMMAND agc_written_buffer_index_tests)
    set_tests_properties(agc_written_buffer_index PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_written_buffer_index PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
