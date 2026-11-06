add_test_executable(agc_command_batch_failure_tests EXCLUDE_FROM_ALL
        tests/CommandBatch.cpp tests/CommandBatchFailure.cpp Graphics/src/Resources.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_command_batch_failure_tests PRIVATE
        ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_command_batch_failure_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_command_batch_failure_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_command_batch_failure_tests)

if(BUILD_TESTING)
    set_target_properties(agc_command_batch_failure_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_command_batch_failure COMMAND agc_command_batch_failure_tests)
    set_tests_properties(agc_command_batch_failure PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_command_batch_failure PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
