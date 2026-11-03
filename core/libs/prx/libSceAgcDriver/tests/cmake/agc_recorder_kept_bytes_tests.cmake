add_test_executable(agc_recorder_kept_bytes_tests EXCLUDE_FROM_ALL tests/RecorderKeptBytes.cpp)
target_include_directories(agc_recorder_kept_bytes_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_recorder_kept_bytes_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_recorder_kept_bytes_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_recorder_kept_bytes_tests)

if(BUILD_TESTING)
    set_target_properties(agc_recorder_kept_bytes_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_recorder_kept_bytes COMMAND agc_recorder_kept_bytes_tests)
    if(WIN32)
        set_tests_properties(agc_recorder_kept_bytes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
