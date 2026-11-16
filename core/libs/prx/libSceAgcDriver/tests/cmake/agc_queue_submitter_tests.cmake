add_test_executable(agc_queue_submitter_tests EXCLUDE_FROM_ALL
        tests/QueueSubmitter.cpp
        Graphics/src/QueueSubmitter.cpp
        Graphics/src/RuntimeSwitches.cpp
        Execution/src/ThreadPriority.cpp
)
target_include_directories(agc_queue_submitter_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_queue_submitter_tests PRIVATE libc ${CMAKE_DL_LIBS})
set_target_properties(agc_queue_submitter_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_queue_submitter_tests)

if(BUILD_TESTING)
    set_target_properties(agc_queue_submitter_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_queue_submitter COMMAND agc_queue_submitter_tests)
    set_tests_properties(agc_queue_submitter PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_queue_submitter PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
