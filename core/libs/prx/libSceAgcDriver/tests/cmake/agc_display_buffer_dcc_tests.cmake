add_test_executable(agc_display_buffer_dcc_tests EXCLUDE_FROM_ALL tests/DisplayBufferDcc.cpp)
target_include_directories(agc_display_buffer_dcc_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_display_buffer_dcc_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_display_buffer_dcc_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_display_buffer_dcc_tests)

if(BUILD_TESTING)
    set_target_properties(agc_display_buffer_dcc_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_display_buffer_dcc COMMAND agc_display_buffer_dcc_tests)
    if(WIN32)
        set_tests_properties(agc_display_buffer_dcc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
